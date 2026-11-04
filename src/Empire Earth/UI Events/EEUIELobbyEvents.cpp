// C:\EEX\Empire Earth\UI Events\EEUIELobbyEvents.cpp (file name inferred: no __FILE__; class prefix EEUIELE). The
// multiplayer/single-player game lobby (form 0x1a) and its two load screens (form 4): 0x1d9d67..0x1e0e1b. Unit flags:
// the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

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
#include "lle/FSPath.h"
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/FSPath.h"
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/UIComboBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIBitmap.h"
#include "lle/UIBackground.h"
#include "lle/NESynchronousServer.h"
#include "lle/FSRestorable.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEARemovePlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameVariant.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUICPing.h"
#include "ee/EEUIELobbyEvents.h"
#include "ee/EEUIEPostGameSummaryEvents.h"
#include "ee/EEStartingResources.h"
#include "LostDebug.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
static unsigned long sStartTime;           // 0x562fd0: the lobby's Start time (provisional name)
static long sPlayerTypeMenus[16];        // 0x562f90: the menu each slot's type box holds (provisional name)
// .data 0x4c6474..0x4c64b0: the string ids of the lobby's difficulty, (unused) 0x4c6480, speed and game type lists
// (LobbyOpen fills the boxes from them, LobbyShowGameOptions labels the choice; provisional names)
static long sDifficultyStrings[3] = { IDS_LOBBY_EASY, IDS_LOBBY_MEDIUM, IDS_LOBBY_HARD };
static long sU4C6480[5] = { 30272, 30273, 30274, 30275, 30276 };
static long sGameSpeedStrings[4] = { IDS_INGAME_SLOW, IDS_STANDARD, IDS_INGAME_FAST, IDS_VERY_FAST };
static long sGameTypeStrings[3] = { IDS_GAMETYPE_RANDOM_MAP, IDS_SAVED_GAME, IDS_GAMETYPE_SCENARIO };

long LobbyFindFile(const UString& inDirectory, const UString& inExtension, UString& outName, long inLoadType);
void LobbyRefresh();
void LobbyUpdateEpochEnd();
bool LobbyCheckStart();
void LobbyFillPlayerTypes(long inMenu, EEGameOptionsPlayer& inPlayer, UIComboBox* inBox);
void LobbyShowRowControls(unsigned long inSlot, unsigned long inReady, unsigned long inTeam, unsigned long inColor,
                          unsigned long inBonus);
void LobbySelectPlayerType(long inMenu, EEGameOptionsPlayer& inPlayer, UIComboBox* inBox);
void LobbyShowClientRow(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost);
void LobbyShowName(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost);
void LobbyShowHostRow(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost);
bool LobbyIsColorUnique(long inColor);
void LobbySetPlayerEvents(UIForm* inForm, long inSlot);
void LobbySetHostEvents(UIForm* inForm);
void LobbySetGameOptionEvents(UIForm* inForm);
void LobbyOpen(UIForm* inForm);
void LobbyUpdateForm();
void LobbyShowPlayer(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost, long inFlags);
void LobbyShowGameOptions(const EEGameOptions& inOptions, const EEGameOptionsPlayer& inHost);
void LobbyOnPlayerTypeYes(UIForm* inForm);
void LobbyOnPlayerTypeNo(UIForm* inForm);
long LobbyRebuildPlayers(EEPSaveGameHeader& inHeader);
long LobbyApplySavedGameOptions(EEPSaveGameHeader& inHeader);

// FUNCTION: EXE 0x5d9d67
// Returns the player type for entry inSelection of lobby player-type menu inMenu; 5 (none) when out of range
// inMenu is the slot's sPlayerTypeMenus value; types: 0 computer, 1 human, 2 observer, 3 open, 4 closed (inferred;
// provisional name). The unit's first function: only ChangePlayerType calls it, and it sits right before CancelLoad
long PlayerTypeFromMenu(unsigned long inSelection, long inMenu)
{
    switch (inMenu) {
    case 5:
        switch (inSelection) {
        case 2:
            return 0;
        case 1:
            return 3;
        case 0:
            return 1;
        }
        break;
    case 4:
        switch (inSelection) {
        case 1:
            return 0;
        case 0:
            return 3;
        }
        break;
    case 3:
        switch (inSelection) {
        case 1:
            return 2;
        case 0:
            return 1;
        }
        break;
    case 2:
        switch (inSelection) {
        case 3:
            return 4;
        case 2:
            return 0;
        case 1:
            return 3;
        case 0:
            return 1;
        }
        break;
    case 1:
        switch (inSelection) {
        case 2:
            return 4;
        case 1:
            return 0;
        case 0:
            return 3;
        }
        break;
    }
    return 5;
}

// FUNCTION: EXE 0x5d9dcf
// Closes the load screen, restores the lobby's previous game type and refreshes the lobby
long EEUIELECancelLoad::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->SetFormOpenEvent(NULL);
    inForm->GetManager()->Pop();
    gGameOptions.mLock->WriteLock();
    gGameOptions.mLoadType = this->mLoadType;
    gGameOptions.mLock->WriteUnlock();
    LobbyRefresh();
    return kErrorOK;
}

// FUNCTION: EXE 0x5d9e19
// Loads the saved game picked on the load screen into the lobby and remembers its name for next time
long EEUIELELoadSavedGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theName;
    bool theFound = LobbyFindFile("Saved Games\\", gSavedGameExtension, theName, 2) == kErrorOK;
    if (theFound)
        gLoadSavedGameName = theName;
    return kErrorOK;
}

// FUNCTION: EXE 0x5d9e82
// Opens the load screen on the Saved Games folder, preselects the last saved game and hooks up Cancel and Load
long EEUIELEOpenLoadSavedGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UWideString theName;
    UString theVolume(gStartupVolume);
    UString theStartup(gStartupDirectory);
    UIForm* theForm = gUIManager->GetFormByID(kFormLoadGame);
    unsigned long theIndex = 0;
    EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
    UIFileListBox* theList = static_cast<UIFileListBox*>(theForm->GetControl(kLoadGameLoadFileListBox));
    theList->AddValidExtension(gSavedGameExtension);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Saved Games\\");
    thePath->AddDirectory(theDirectory, true);
    theList->SetSearchPaths(thePath);
    theList->CreateEntries(1);
    theName = gLoadSavedGameName;
    if (theList->FindString(theName, theIndex) == true) {
        theList->ChangeSelectionIndex(theIndex, true, false);
        theList->SetScrollBarCurrentRange(theIndex);
    }
    thePath->Dereference();
    UIEvent* theEvent = new EEUIELECancelLoad;
    inForm->GetControl(kLoadGameCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELELoadSavedGame;
    inForm->GetControl(kLoadGameLoadButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    inForm->GetControl(kLoadGameLoadFileListBox)->SetTriggerEvent(kControlTriggerDoubleClicked, theEvent);
    theEvent->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5da0c5
// Destroys the cancel-load event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIELECancelLoad and EEUIELELoadSavedGame, after their first user)

// FUNCTION: EXE 0x5da0f0
// Destroys the load-saved-game event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5da11b
// Loads the scenario picked on the load screen into the lobby and remembers its name for next time
long EEUIELELoadScenarioGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theName;
    bool theFound = LobbyFindFile("Scenarios\\", gScenarioExtension, theName, 1) == kErrorOK;
    if (theFound)
        gLoadScenarioName = theName;
    return kErrorOK;
}

// FUNCTION: EXE 0x5da184
// Opens the load screen on the Scenarios folder titled Play Scenario, preselects the last scenario and hooks
// up Cancel and Load
long EEUIELEOpenLoadScenarioGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UWideString theTitle;
    UWideString theName;
    UString theVolume(gStartupVolume);
    UString theStartup(gStartupDirectory);
    UIForm* theForm = gUIManager->GetFormByID(kFormLoadGame);
    unsigned long theIndex = 0;
    EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
    UIFileListBox* theList = static_cast<UIFileListBox*>(theForm->GetControl(kLoadGameLoadFileListBox));
    theList->AddValidExtension(gScenarioExtension);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    thePath->AddDirectory(theDirectory, true);
    theList->SetSearchPaths(thePath);
    theList->CreateEntries(1);
    theName = gLoadScenarioName;    // the last scenario (it preselected the last saved game's name)
    if (theList->FindString(theName, theIndex) == true) {
        theList->ChangeSelectionIndex(theIndex, true, false);
        theList->SetScrollBarCurrentRange(theIndex);
    }
    thePath->Dereference();
    UIEvent* theEvent = new EEUIELECancelLoad;
    inForm->GetControl(kLoadGameCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELELoadScenarioGame;
    inForm->GetControl(kLoadGameLoadButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    inForm->GetControl(kLoadGameLoadFileListBox)->SetTriggerEvent(kControlTriggerDoubleClicked, theEvent);
    theEvent->Dereference();
    gResource->GetString(IDS_PLAY_SCENARIO, theTitle);
    static_cast<UITextLabel*>(inForm->GetControl(kLoadGameLoadScreenTitle))->SetText(theTitle);
    return kErrorOK;
}

// FUNCTION: EXE 0x5da3ff
// Destroys the load-scenario event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5da42a
// Sets a slot's starting citizens (1 to 20) from its bonus box and sends the change
long EEUIELEChangeCitizenBonus::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEGameOptionsPlayer thePlayer;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        thePlayer = gWorld.GetLobbyOptions().GetPlayer(theBox->GetIndex() - 126);
        if (thePlayer.mStartingCitizens - 1 != theSelection) {
            thePlayer.mStartingCitizens = theSelection + 1;
            gCommunications->ProcessGuaranteedLocalUserInput(
                new EEMPlayerInformation(4, thePlayer.mSlot, thePlayer.mStartingCitizens, 0.0f), kSlotNone);
            gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
        }
    }
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5da5a2
// Sends the chat box's line, prefixed with the local player's name, as a lobby chat message and clears the box
long EEUIELEChat::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText(this->mPrefix);
    UIEditBox* theBox = static_cast<UIEditBox*>(inEntity);
    UWideString theLine(theBox->GetText(0));
    if (theLine.GetLength() > 0) {
        theText = this->mPrefix + ": " + theLine;
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMChat(0, theText), kSlotNone);
    }
    theBox->DeleteText();
    return kErrorOK;
}

// FUNCTION: EXE 0x5da661
// Creates a chat message for the given recipients with the given text
// (EEMChat's inline ctor, emitted out of line after its first user)

// FUNCTION: EXE 0x5da4db
// Leaves the lobby: rejoins the WON room, then disconnects as a client or removes the host itself; closes the
// form unless the room rejoin failed
long EEUIELECancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    bool theClose = true;
    if (gWonNet) {
        gWonNet->ClearGameList();
        theClose = gWonNet->RejoinRoom(true);
    }
    if (gCommunications->IsClient())
        gCommunications->ProcessGuaranteedLocalUserInput(new NEMClientDisconnectionNotification(false), kSlotNone);
    else {
        EEAction* theAction = new EEARemovePlayer(0xfe);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    }
    if (theClose)
        gUserInterface->CloseForm(NULL);
    return kErrorOK;
}
// NEAR: the original inlines EEARemovePlayer's ctor, ours refuses it even in a function of its own; with two of its
// three member stores it inlines, so the original's ctor chain is cheaper than ours

// FUNCTION: EXE 0x5da6b3
// Starts the game when the lobby checks pass: closes the server to new players, updates the WON game info, sends
// start loading with a random seed, saves the options as the last game's and hides Start
long EEUIELEStart::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEGameOptionsPlayer thePlayer;
    EEWonNetGameInfo theInfo;
    unsigned char theHumans = 0;
    unsigned char thePlayers = 0;
    gGameOptions.mLock->ReadLock();
    if (LobbyCheckStart()) {
        sStartTime = TMGetTime();
        static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
        static_cast<EEServer*>(gCommunications)->SetGameState(2);
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        for (unsigned char i = 0; i < 16; i++) {
            thePlayer = gGameOptions.mPlayers[i];
            if (thePlayer.mType == 1 || thePlayer.mType == 2)
                theHumans++;
            if (thePlayer.mType == 3 || thePlayer.mType == 1 || thePlayer.mType == 2)
                thePlayers++;
        }
        if (gWonNet) {
            gWonNet->GetGameInfo(theInfo);
            theInfo.mFlags &= ~1;
            theInfo.mPlayers = thePlayers;
            theInfo.mHumans = theHumans;
            gWonNet->SetGameInfo(theInfo);
        }
        static_cast<EEServer*>(gCommunications)->StopPing();
        static_cast<EEServer*>(gCommunications)->ResetXfer();
        static_cast<EEServer*>(gCommunications)->mLoadState = true;
        static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
        srand(TMGetTime());
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMStartLoading(theHumans, rand()), kSlotNone);
        gGamePreferences.SetLastGameOptions(gGameOptions);
        inForm->GetControl(kLobbyStartGameButton)->SetDisplayMode(kDisplayModeHidden);
    }
    gGameOptions.mLock->ReadUnlock();
    return kErrorOK;
    EE_LOST_DEBUG(1);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x5da876
// Checks whether the game may start (load file opens and matches; two players and a human; unique human colours; not
// all one team; everyone ready; no file transfers left), showing the reason when not
bool LobbyCheckStart()
{
    EEGameOptions theOptions;
    EEPSaveGameHeader theHeader;
    std::vector<UWideString> theMissing;
    UWideString theText;
    UString theFile;
    UString theDirectory;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    EEGameOptionsPlayer thePlayer;
    unsigned long theCount = 0;
    unsigned long theHumans = 0;
    bool theResult = true;
    if (gGameOptions.mLoadType != 0) {
        if (gGameOptions.mLoadType == 2)
            theDirectory = "Saved Games\\";
        else
            theDirectory = "Scenarios\\";
        theFile = gGameOptions.mMapType;
        FSFileSpec* theStream;
        long theError = EEOpenSaveGameFile(theFile, theDirectory, theStream);
        if (theError == kErrorOK) {
            theHeader.mMajorVersion = kSaveGameMajorVersion;
            theHeader.mMinorVersion = kSaveGameMinorVersion;
            theError = theHeader.LoadFromFile(*theStream);
            if (theError == kErrorOK) {
                theOptions = theHeader.mGameOptions;
                if (!EECheckSavedGameOptions(theOptions, gGameOptions, true, theMissing, theHeader.mNumPlayers - 1)) {
                    theText = "";
                    for (unsigned long i = 0; i < theMissing.size(); i++)
                        theText += theMissing[i] + "\n";
                    gUIManager->DisplayMessage(theText, true, NULL, 0, true);
                    theError = kErrorFailed;
                }
            } else {
                gResource->GetString(IDS_THERE_WAS_ERROR_READING_FILE, theText);
                gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            }
        } else {
            gResource->GetString(IDS_FILE_NAME_NOT_FOUND, theText);
            gUIManager->DisplayMessage(theText, true, NULL, 0, true);
        }
        theResult = theError == kErrorOK;
    } else {
        for (long i = 0; i < 16; i++) {
            thePlayer = gWorld.GetLobbyOptions().GetPlayer(i);
            if (thePlayer.mType != 0 && thePlayer.mType != 1)
                continue;
            theCount++;
            if (theCount > 1 && theHumans > 0)
                break;
            if (thePlayer.mType == 1)
                theHumans++;
        }
        if (theCount > 1) {
            if (theHumans < 1) {
                theResult = false;
                gResource->GetString(IDS_YOU_NEED_AT_LEAST_1_HUMAN, theText);
                gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            }
        } else {
            theResult = false;
            if (gGameOptions.mIsMultiplayer == true)
                gResource->GetString(IDS_YOU_CANT_PLAY_GAME_WITH_ONLY_ONE_PLAYER, theText);
            else
                gResource->GetString(IDS_YOU_CANT_PLAY_GAME_WITH_ONLY_ONE_PLAYER_274F, theText);
            gUIManager->DisplayMessage(theText, true, NULL, 0, true);
        }
        bool theColorUsed[16] = { false };
        for (unsigned char j = 0; j < 16; j++) {
            thePlayer = gGameOptions.mPlayers[j];
            if (thePlayer.mType != 1)
                continue;
            if (theColorUsed[thePlayer.mColor] == true) {
                theResult = false;
                gResource->GetString(IDS_ONE_OR_MORE_PLAYERS_HAVE_SAME, theText);
                gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            } else
                theColorUsed[thePlayer.mColor] = true;
        }
        if (theResult == true) {
            thePlayer = gGameOptions.mPlayers[0];
            long theTeam = thePlayer.mTeam;
            if (theTeam != 0) {
                bool theFound = false;
                for (long k = 1; k < 16; k++) {
                    thePlayer = gWorld.GetLobbyOptions().GetPlayer(k);
                    if ((thePlayer.mType == 0 || thePlayer.mType == 1)
                        && (thePlayer.mTeam == 0 || theTeam != thePlayer.mTeam)) {
                        theFound = true;
                        break;
                    }
                }
                if (!theFound) {
                    theResult = false;
                    gResource->GetString(IDS_ALL_PLAYERS_CANT_BE_ON_SAME, theText);
                    gUIManager->DisplayMessage(theText, true, NULL, 0, true);
                }
            }
        }
    }
    if (theResult == true && gGameOptions.mIsMultiplayer == true) {
        for (long i = 0; i < 16; i++) {
            thePlayer = gGameOptions.mPlayers[i];
            if ((thePlayer.mType != 1 && thePlayer.mType != 2) || thePlayer.mIsReady != false)
                continue;
            theResult = false;
            gResource->GetString(IDS_CANT_START_GAME_UNTIL_ALL_PLAYERS, theText);
            gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            break;
        }
    }
    if (theResult == true) {
        for (unsigned char i = 1; i < 16; i++) {
            thePlayer = gGameOptions.mPlayers[i];
            if ((thePlayer.mType != 1 && thePlayer.mType != 2) || !(thePlayer.mTransferID > 0) || theResult != true)
                continue;
            gResource->GetString(IDS_YOU_CANT_START_GAME_UNTIL, theText);
            gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            theResult = false;
        }
    } else if (gGameOptions.mLoadType != 0)
        for (long i = 257; i < 272; i++)
            theForm->GetControl(i)->SetDisplayMode(kDisplayModeHidden);
    return theResult;
}

// FUNCTION: EXE 0x5dad59
// Steps a slot's colour to the next of 16 (wrapping), shows it on the button and sends the change
long EEUIELEIncChangeColor::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEGameOptionsPlayer thePlayer;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    UIControl* theControl = static_cast<UIControl*>(inEntity);
    thePlayer = gWorld.GetLobbyOptions().GetPlayer(theControl->GetIndex() - 78);
    this->mSlot++;
    if (this->mSlot >= 16)
        this->mSlot = 0;
    theControl->SetBgColor(EEGetPlayerColor(this->mSlot));
    thePlayer.mColor = this->mSlot;
    gCommunications->ProcessGuaranteedLocalUserInput(
        new EEMPlayerInformation(2, thePlayer.mSlot, thePlayer.mColor, 0.0f), kSlotNone);
    static_cast<EEUIELEDecChangeColor*>(theControl->GetTriggerEvent(kControlTriggerRightButtonUp))->SetSlot(this->mSlot);
    gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dae32
// Steps a slot's colour to the previous of 16 (wrapping), shows it on the button and sends the change
long EEUIELEDecChangeColor::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEGameOptionsPlayer thePlayer;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    UIControl* theControl = static_cast<UIControl*>(inEntity);
    thePlayer = gWorld.GetLobbyOptions().GetPlayer(theControl->GetIndex() - 78);
    if (this->mSlot == 0)
        this->mSlot = 15;
    else
        this->mSlot--;
    theControl->SetBgColor(EEGetPlayerColor(this->mSlot));
    thePlayer.mColor = this->mSlot;
    gCommunications->ProcessGuaranteedLocalUserInput(
        new EEMPlayerInformation(2, thePlayer.mSlot, thePlayer.mColor, 0.0f), kSlotNone);
    static_cast<EEUIELEIncChangeColor*>(theControl->GetTriggerEvent(kControlTriggerButtonUp))->SetSlot(this->mSlot);
    gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5daf10
// Sets a slot's team from its team box and sends the change
long EEUIELEChangeTeam::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEGameOptionsPlayer thePlayer;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    thePlayer = gWorld.GetLobbyOptions().GetPlayer(theBox->GetIndex() - 62);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        thePlayer.mTeam = theSelection;
        gCommunications->ProcessGuaranteedLocalUserInput(
            new EEMPlayerInformation(3, thePlayer.mSlot, thePlayer.mTeam, 0.0f), kSlotNone);
        gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
    }
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dafb8
// Changes a slot's type from its type box: the host asks before booting a human from a slot, other changes apply
// and are sent at once; a client can only switch its own slot between player and observer
long EEUIELEChangePlayerType::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    EEGameOptionsPlayer thePlayer;
    EEGameOptionsPlayer theHost;
    bool theAsk = false;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    thePlayer = gWorld.GetLobbyOptions().GetPlayer(theBox->GetIndex() - 30);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        bool theChanged = false;
        if (gCommunications->IsServer() == true && thePlayer.mIsLocal == false) {
            theChanged = thePlayer.mType != PlayerTypeFromMenu(theSelection, sPlayerTypeMenus[thePlayer.mSlot]);
            if (theChanged == true) {
                if (thePlayer.mType == 1 || thePlayer.mType == 2) {
                    theAsk = true;
                    theSelection--;
                }
                thePlayer.mType = PlayerTypeFromMenu(theSelection, sPlayerTypeMenus[thePlayer.mSlot]);
            }
        } else {
            switch (thePlayer.mType) {
            case 1:
                theChanged = theSelection != 0;
                break;
            case 2:
                theChanged = theSelection != 1;
                break;
            }
            if (theChanged == true) {
                switch (theSelection) {
                case 0:
                    thePlayer.mType = 1;
                    break;
                case 1:
                    thePlayer.mType = 2;
                    break;
                }
            }
        }
        if (theChanged == true) {
            if (theAsk == true) {
                EEGameOptionsPlayer* theCopy = new EEGameOptionsPlayer;
                *theCopy = thePlayer;
                gResource->GetString(IDS_BOOT_THIS_PLAYER, theText);
                gUIManager->DisplayQuestion(theText, true, reinterpret_cast<UIForm*>(theCopy), LobbyOnPlayerTypeYes, LobbyOnPlayerTypeNo);
            }
            if (theAsk == false) {
                gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
                theHost = gWorld.GetLobbyOptions().mPlayers[0];
                LobbyShowPlayer(thePlayer, theHost, 0);
                gCommunications->ProcessGuaranteedLocalUserInput(
                    new EEMPlayerInformation(0, thePlayer.mSlot, thePlayer.mType, 0.0f), kSlotNone);
            }
        }
    }
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5db19d
// Answers no to the boot question: puts the slot's type box back on its first entry and frees the player copy
// inForm is the EEGameOptionsPlayer copy ChangePlayerType passed; only the memory is freed (operator delete
// without the dtor: the callback type takes a UIForm*)
void LobbyOnPlayerTypeNo(UIForm* inForm)
{
    EEGameOptionsPlayer* thePlayer = reinterpret_cast<EEGameOptionsPlayer*>(inForm);
    static_cast<UIComboBox*>(gUIManager->GetFormByID(kFormLobby)->GetControl(thePlayer->mSlot + 30))->SetCurrentSelection(0);
    delete static_cast<void*>(inForm);
}


// FUNCTION: EXE 0x5db1ce
// Answers yes to the boot question: boots the slot's client, resets the slot (not ready, default citizens, no name
// or team) to the chosen type and adjusts the WON game's human and player counts
// inForm is the EEGameOptionsPlayer copy ChangePlayerType passed (provisional name)
void LobbyOnPlayerTypeYes(UIForm* inForm)
{
    UWideString theText;
    UString theString;
    EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
    EEGameOptionsPlayer thePlayer(*reinterpret_cast<EEGameOptionsPlayer*>(inForm));
    EEWonNetGameInfo theInfo;
    delete static_cast<void*>(inForm);
    gGameOptions.mLock->WriteLock();
    thePlayer.mIsReady = false;
    thePlayer.mStartingCitizens = gWorldTable.GetLongValue(48);
    thePlayer.mName = "";
    thePlayer.mPing = 0.0f;
    thePlayer.mTeam = 0;
    if (gWonNet)
        gWonNet->GetGameInfo(theInfo);
    if (thePlayer.mType != 0)
        theInfo.mHumans--;
    if (thePlayer.mType == 4)
        theInfo.mPlayers--;
    theServer->RefuseClient(thePlayer);
    if (gWonNet)
        gWonNet->SetGameInfo(theInfo);
    gGameOptions.mPlayers[thePlayer.mSlot] = thePlayer;
    gGameOptions.mLock->WriteUnlock();
    EE_LOST_DEBUG(4);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x5db2fc
// Copies a lobby slot's settings
// (EEGameOptionsPlayer's implicit copy ctor, after its first user)

// FUNCTION: EXE 0x5db358
// Sets the local player ready or not (the host also its computer slots), refusing ready on a taken colour; sends
// it, redraws the rows and hides Cancel while ready
long EEUIELEChangeReady::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    EEGameOptionsPlayer thePlayer;
    EEGameOptionsPlayer theOther;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    thePlayer = gWorld.GetLobbyOptions().mPlayers[theBox->GetIndex() - 12];
    if (theBox->GetIsChecked() != thePlayer.mIsReady) {
        if (theBox->GetIsChecked() == true && !LobbyIsColorUnique(thePlayer.mColor)) {
            gResource->GetString(IDS_PLAYER_COLOR_ALREADY_SELECTED, theText);
            gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            theBox->SetChecked(theBox->GetIsChecked() == false);
        } else if (gCommunications->IsServer() == true) {
            for (unsigned char i = 0; i < 16; i++) {
                thePlayer = gGameOptions.mPlayers[i];
                if (!thePlayer.mIsLocal && thePlayer.mType != 0)
                    continue;
                thePlayer.mIsReady = theBox->GetIsChecked();
                gGameOptions.GetPlayer(thePlayer.mSlot) = thePlayer;
                gCommunications->ProcessGuaranteedLocalUserInput(
                    new EEMPlayerInformation(5, thePlayer.mSlot, thePlayer.mIsReady == true, 0.0f), kSlotNone);
            }
        } else {
            thePlayer.mIsReady = theBox->GetIsChecked();
            gGameOptions.GetPlayer(thePlayer.mSlot) = thePlayer;
            gCommunications->ProcessGuaranteedLocalUserInput(
                new EEMPlayerInformation(5, thePlayer.mSlot, thePlayer.mIsReady == true, 0.0f), kSlotNone);
        }
        if (gCommunications->IsServer() == true) {
            thePlayer = gGameOptions.mPlayers[0];
            thePlayer.mIsReady = theBox->GetIsChecked();
            for (unsigned char i = 1; i < 16; i++) {
                theOther = gGameOptions.mPlayers[i];
                if (theOther.mType == 0)
                    theOther.mIsReady = thePlayer.mIsReady;
                LobbyShowPlayer(theOther, thePlayer, 8);
            }
            LobbyShowPlayer(thePlayer, thePlayer, 8);
            LobbyShowGameOptions(gGameOptions, thePlayer);
        } else {
            theOther = gGameOptions.mPlayers[0];
            LobbyShowPlayer(thePlayer, theOther, 8);
        }
        if (theBox->GetIsChecked() == true)
            inForm->GetControl(kLobbyCancelButton)->SetDisplayMode(kDisplayModeHidden);
        else
            inForm->GetControl(kLobbyCancelButton)->SetDisplayMode(kDisplayModeShown);
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}
// NEAR: open on the slot-index fold


// FUNCTION: EXE 0x5db5e4
// Returns whether at most one human uses the colour (always true for a loaded game)
bool LobbyIsColorUnique(long inColor)
{
    EEGameOptionsPlayer thePlayer;
    unsigned long theCount = 0;
    if (gGameOptions.mLoadType == 0) {
        for (unsigned char i = 0; i < 16; i++) {
            thePlayer = gGameOptions.mPlayers[i];
            if (thePlayer.mColor == inColor && thePlayer.mType == 1)
                theCount++;
        }
    }
    return theCount <= 1;
}


// FUNCTION: EXE 0x5db63c
// Sets the victory option from its check box and sends the change
long EEUIELEChangeVictory::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mVictory) {
        NEMessage* theMessage = new EEMGameOptions(17, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mVictory = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5db6ec
// Refreshes the visible lobby: trims the end-epoch list, selects the end epoch, hooks up the local slots' (and the
// host's option) events and enables the form
void LobbyUpdateForm()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    if (!gUIManager->IsFormVisible(theForm))
        return;
    gGameOptions.mLock->ReadLock();
    LobbyUpdateEpochEnd();
    long theEnd = gGameOptions.mEpochEnd;
    if (gGameOptions.mEpochStart != kTechEpochAll)
        theEnd -= gGameOptions.mEpochStart;
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyEpochEndCombo))->SetCurrentSelection(theEnd);
    for (unsigned char i = 0; i < 16; i++)
        if (gGameOptions.mPlayers[i].mIsLocal == true)
            LobbySetPlayerEvents(theForm, i);
    if (gCommunications->IsServer() == true) {
        LobbySetGameOptionEvents(theForm);
        LobbySetHostEvents(theForm);
    }
    theForm->GetEntityGroup(1)->Enable();
    gGameOptions.mLock->ReadUnlock();
}


// FUNCTION: EXE 0x5db7a7
// Hooks up a slot's type, colour (left/right click), team and ready controls
void LobbySetPlayerEvents(UIForm* inForm, long inSlot)
{
    UIEvent* theEvent = new EEUIELEChangePlayerType;
    inForm->GetControl(inSlot + 30)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEIncChangeColor(gDefaultPlayerColors[inSlot]);
    inForm->GetControl(inSlot + 78)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEDecChangeColor(gDefaultPlayerColors[inSlot]);
    inForm->GetControl(inSlot + 78)->SetTriggerEvent(kControlTriggerRightButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEChangeTeam;
    inForm->GetControl(inSlot + 62)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEChangeReady;
    inForm->GetControl(inSlot + 12)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
}

// FUNCTION: EXE 0x5db91a
// Destroys the increase-colour event and frees it when asked (scalar deleting destructor)
// (??_G of IncChangeColor, DecChangeColor, ChangeTeam, ChangePlayerType, ChangeReady: declaration order)

// FUNCTION: EXE 0x5db945
// Destroys the decrease-colour event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5db970
// Destroys the change-team event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5db99b
// Destroys the change-player-type event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5db9c6
// Destroys the change-ready event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5db9f1
// Hooks up the host's type boxes for slots 1 to 15 and the citizen bonus boxes of all slots
void LobbySetHostEvents(UIForm* inForm)
{
    UIEvent* theEvent = new EEUIELEChangePlayerType;
    for (long i = 31; i < 46; i++)
        inForm->GetControl(i)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEChangeCitizenBonus;
    for (i = 126; i < 142; i++)
        inForm->GetControl(i)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
}

// FUNCTION: EXE 0x5dbaad
// Destroys the change-citizen-bonus event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dbad8
// Hooks up the host's game option combo boxes and check boxes
void LobbySetGameOptionEvents(UIForm* inForm)
{
    UIEvent* theEvent = new EEUIELEGameType;
    inForm->GetControl(kLobbyGameTypeCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameMapSize;
    inForm->GetControl(kLobbyMapSizeCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameMapType;
    inForm->GetControl(kLobbyMapTypeCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameResources;
    inForm->GetControl(kLobbyResourcesCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameEpochStart;
    inForm->GetControl(kLobbyEpochStartCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEEpochEnd;
    inForm->GetControl(kLobbyEpochEndCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameUnitLimit;
    inForm->GetControl(kLobbyWorldUnitLimitCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameWondersForVictory;
    inForm->GetControl(kLobbyWondersVictoryCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameVariant;
    inForm->GetControl(kLobbyGameVariantCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameDifficulty;
    inForm->GetControl(kLobbyDifficultyCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEGameSpeed;
    inForm->GetControl(kLobbyGameSpeedCombo)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    UIEvent* theCheckBoxEvent = new EEUIELEGameRecordGames;
    inForm->GetControl(kLobbyGameOpCheckBox6RecGame)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox6RecGame)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
    theCheckBoxEvent = new EEUIELEGameRevealMap;
    inForm->GetControl(kLobbyGameOpCheckBox1RevealMap)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox1RevealMap)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
    theCheckBoxEvent = new EEUIELEGameTeamIslands;
    inForm->GetControl(kLobbyGameOpCheckBox2CustomCiv)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox2CustomCiv)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
    theCheckBoxEvent = new EEUIELEGameLockTeams;
    inForm->GetControl(kLobbyGameOpCheckBox3LockTeam)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox3LockTeam)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
    theCheckBoxEvent = new EEUIELEGameLockSpeed;
    inForm->GetControl(kLobbyGameOpCheckBox4LockSpeed)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox4LockSpeed)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
    theCheckBoxEvent = new EEUIELEGameCheatCodes;
    inForm->GetControl(kLobbyGameOpCheckBox5Cheats)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox5Cheats)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
    theCheckBoxEvent = new EEUIELEChangeVictory;
    inForm->GetControl(kLobbyGameOpCheckBox7Victory)->SetTriggerEvent(kControlTriggerChecked, theCheckBoxEvent);
    inForm->GetControl(kLobbyGameOpCheckBox7Victory)->SetTriggerEvent(kControlTriggerUnchecked, theCheckBoxEvent);
    theCheckBoxEvent->Dereference();
}

// FUNCTION: EXE 0x5dbfe8
// Destroys the game-type option event and frees it when asked (scalar deleting destructor)
// (??_G of the game option events and ChangeVictory, in declaration order)

// FUNCTION: EXE 0x5dc013
// Destroys the map-size option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc03e
// Destroys the map-type option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc069
// Destroys the resources option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc094
// Destroys the start-epoch option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc0bf
// Destroys the end-epoch option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc0ea
// Destroys the unit-limit option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc115
// Destroys the wonders-for-victory option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc140
// Destroys the game-variant option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc16b
// Destroys the difficulty option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc196
// Destroys the game-speed option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc1c1
// Destroys the record-games option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc1ec
// Destroys the custom-civs option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc217
// Destroys the reveal-map option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc242
// Destroys the lock-teams option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc26d
// Destroys the lock-speed option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc298
// Destroys the cheat-codes option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc2c3
// Destroys the victory option event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5dc2ee
// Loads the picked file's header, checks its options against the lobby's, sends the file to the joined humans and
// applies it; shows the problem and returns an error otherwise
// The load screens' Load button (provisional name)
long LobbyFindFile(const UString& inDirectory, const UString& inExtension, UString& outName, long inLoadType)
{
    EEPSaveGameHeader theHeader;
    EEGameOptionsPlayer thePlayer;
    EEGameOptions theOptions;
    EEGameOptions theCurrent;
    std::vector<UWideString> theMissing;
    UWideString theText;
    UWideString theUnused;
    UString theName;
    UString theFile;
    FSFileSpec* theStream = NULL;
    UIFileListBox* theList = static_cast<UIFileListBox*>(gUIManager->GetFormByID(kFormLoadGame)->GetControl(kLoadGameLoadFileListBox));
    bool theDone = false;
    long theError;
    gWorld.GetLobbyOptions().mLock->WriteLock();
    unsigned long theSelection;
    theError = theList->GetCurrentSelection(theSelection);
    if (theError == kErrorOK) {
        theName = theList->GetData(theSelection);
        theFile = theName + inExtension;
        theError = EEOpenSaveGameFile(theFile, inDirectory, theStream);
        if (theError == kErrorOK) {
            theHeader.SetMajorVersion(kSaveGameMajorVersion);
            theHeader.SetMinorVersion(kSaveGameMinorVersion);
            theError = theHeader.LoadFromFile(*theStream);
            if (theError == kErrorOK) {
                unsigned long theChecksum = theHeader.mChecksum;
                outName = theName;
                theOptions = theHeader.mGameOptions;
                theCurrent = gGameOptions;
                theCurrent.mLoadType = inLoadType;
                if (EECheckSavedGameOptions(theOptions, theCurrent, false, theMissing, theHeader.mNumPlayers - 1) == true) {
                    for (long i = 1; i < 16; i++) {
                        thePlayer = gGameOptions.mPlayers[i];
                        if (thePlayer.mType != 1 && thePlayer.mType != 2)
                            continue;
                        static_cast<NESynchronousServer*>(gCommunications)->AbortXfer(thePlayer.mTransferID);
                        thePlayer.mTransferID = 0;
                        static_cast<NESynchronousServer*>(gCommunications)->XferFileToClient(*theStream, theChecksum,
                            static_cast<EEServer*>(gCommunications)->GetSendToSlot(i), thePlayer.mTransferID, false);
                        gWorld.GetLobbyOptions().GetPlayer(i) = thePlayer;
                    }
                    gWorld.GetLobbyOptions().mLoadType = inLoadType;
                    gWorld.GetLobbyOptions().mMapType = UWideString(theFile);
                    theError = LobbyApplySavedGameOptions(theHeader);
                    if (theError == kErrorOK)
                        theError = LobbyRebuildPlayers(theHeader);
                    theDone = true;
                } else {
                    theText = "";
                    for (unsigned long i = 0; i < theMissing.size(); i++)
                        theText += theMissing[i] + "\n";
                    gUIManager->DisplayMessage(theText, true, NULL, 0, true);
                    theError = 9;
                }
            } else {
                gResource->GetString(IDS_THERE_WAS_ERROR_READING_FILE, theText);
                gUIManager->DisplayMessage(theText, true, NULL, 0, true);
            }
        } else {
            gResource->GetString(IDS_FILE_NAME_NOT_FOUND, theText);
            gUIManager->DisplayMessage(theText, true, NULL, 0, true);
        }
    }
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
    if (theDone == true) {
        gUIManager->GetFormByID(kFormLoadGame)->SetFormOpenEvent(NULL);
        gUIManager->GetFormByID(kFormLoadGame)->GetManager()->Pop();
        LobbyRefresh();
    }
    return theError;
}

// FUNCTION: EXE 0x5dc6be
// Rebuilds the slots from a loaded game's header and sends them: joined humans keep their slots (matched by name in
// saved games), the others become computer, open or closed; returns 9 when the humans don't fit
// (provisional name)
long LobbyRebuildPlayers(EEPSaveGameHeader& inHeader)
{
    std::vector<EEGameOptionsPlayer> theOthers;
    std::vector<EEGameOptionsPlayer> theHumans;
    EEGameOptionsPlayer theHost;
    EEGameOptionsPlayer theSaved;
    EEGameOptionsPlayer thePlayer;
    for (long i = 0; i < 16; i++) {
        thePlayer = gWorld.GetLobbyOptions().mPlayers[15 - i];
        if (thePlayer.mType == 1 || thePlayer.mType == 2)
            theHumans.push_back(thePlayer);
        else {
            thePlayer.mType = 4;
            theOthers.push_back(thePlayer);
        }
    }
    // Fix: every saved slot that is not a joined human takes an entry from theOthers, and nothing checked that one
    // is left (a saved player matching no joined human used one too many and read and wrote before the start of
    // the list). Running out now stops the rebuild; the humans left over then make it fail with 9.
    bool theOutOfSlots = false;
    long theResult = theHumans.size() > 0 ? kErrorOK : 9;
    if (theResult == kErrorOK) {
        if (gWorld.GetLobbyOptions().mLoadType == 2) {
            for (long j = 0; j < 16; j++) {
                theSaved = inHeader.mGameOptions.mPlayers[j];
                bool theFound = false;
                if (theSaved.mType == 1 || theSaved.mType == 2) {
                    for (unsigned long k = 0; k < theHumans.size(); k++) {
                        if (theHumans[k].mName == theSaved.mName) {
                            theHumans[k].mType = theSaved.mType;
                            theHumans[k].mLoadedType = theSaved.mLoadedType;
                            theHumans[k].mStartingCitizens = theSaved.mStartingCitizens;
                            theHumans[k].mColor = theSaved.mColor;
                            theHumans[k].mTeam = theSaved.mTeam;
                            theHumans[k].mB36 = theSaved.mB36;
                            gWorld.GetLobbyOptions().mPlayers[theHumans[k].mSlot] = theHumans[k];
                            theHumans.erase(theHumans.begin() + k);
                            theFound = true;
                            break;
                        }
                    }
                }
                if (theFound)
                    continue;
                if (theOthers.empty()) {
                    theOutOfSlots = true;
                    break;
                }
                long theType;
                if (theSaved.mType == 1 || theSaved.mType == 2)
                    theType = 3;
                else
                    theType = theSaved.mType != 0 ? 4 : 0;
                theOthers.back().mType = theType;
                theOthers.back().mName = theSaved.mName;
                theOthers.back().mStartingCitizens = theSaved.mStartingCitizens;
                theOthers.back().mLoadedType = theSaved.mType;
                theOthers.back().mColor = theSaved.mColor;
                theOthers.back().mTeam = theSaved.mTeam;
                theOthers.back().mB36 = theSaved.mB36;
                gGameOptions.mPlayers[theOthers.back().mSlot] = theOthers.back();
                theOthers.pop_back();
            }
        } else {
            theSaved = inHeader.mGameOptions.mPlayers[0];
            theHumans.back().mStartingCitizens = theSaved.mStartingCitizens;
            theHumans.back().mColor = theSaved.mColor;
            theHumans.back().mTeam = theSaved.mTeam;
            theHumans.back().mB36 = theSaved.mB36;
            gWorld.GetLobbyOptions().mPlayers[theHumans.back().mSlot] = theHumans.back();
            theHumans.pop_back();
            for (long j = 1; j < 16; j++) {
                if (j < (long)inHeader.mNumPlayers - 1) {
                    theSaved = inHeader.mGameOptions.mPlayers[j];
                    if (theSaved.mB36 == true) {
                        if (theOthers.empty()) {
                            theOutOfSlots = true;
                            break;
                        }
                        theOthers.back().mType = 0;
                        theOthers.back().mName = theSaved.mName;
                        theOthers.back().mStartingCitizens = theSaved.mStartingCitizens;
                        theOthers.back().mLoadedType = theSaved.mType;
                        theOthers.back().mColor = theSaved.mColor;
                        theOthers.back().mTeam = theSaved.mTeam;
                        theOthers.back().mB36 = theSaved.mB36;
                        gWorld.GetLobbyOptions().mPlayers[theOthers.back().mSlot] = theOthers.back();
                        theOthers.pop_back();
                    } else if (theHumans.size() > 0) {
                        theHumans.back().mStartingCitizens = theSaved.mStartingCitizens;
                        theHumans.back().mColor = theSaved.mColor;
                        theHumans.back().mTeam = theSaved.mTeam;
                        theHumans.back().mB36 = theSaved.mB36;
                        gWorld.GetLobbyOptions().GetPlayer(theHumans.back().mSlot) = theHumans.back();
                        theHumans.pop_back();
                    } else {
                        if (theOthers.empty()) {
                            theOutOfSlots = true;
                            break;
                        }
                        theOthers.back().mType = 3;
                        theOthers.back().mName = theSaved.mName;
                        theOthers.back().mStartingCitizens = theSaved.mStartingCitizens;
                        theOthers.back().mLoadedType = theSaved.mType;
                        theOthers.back().mColor = theSaved.mColor;
                        theOthers.back().mTeam = theSaved.mTeam;
                        theOthers.back().mB36 = theSaved.mB36;
                        gWorld.GetLobbyOptions().mPlayers[theOthers.back().mSlot] = theOthers.back();
                        theOthers.pop_back();
                    }
                } else {
                    if (theOthers.empty()) {
                        theOutOfSlots = true;
                        break;
                    }
                    theOthers.back().mType = 4;
                    theOthers.back().mLoadedType = 4;
                    theOthers.back().mB36 = false;
                    gWorld.GetLobbyOptions().mPlayers[theOthers.back().mSlot] = theOthers.back();
                    theOthers.pop_back();
                }
            }
        }
    }
    theResult = theHumans.size() != 0 || theOutOfSlots ? 9 : kErrorOK;
    if (theResult == kErrorOK) {
        for (long j = 0; j < 16; j++) {
            gCommunications->ProcessGuaranteedLocalUserInput(new EEMFullPlayerInformation(gWorld.GetLobbyOptions().mPlayers[j]),
                                                             kSlotNone);
            thePlayer = gWorld.GetLobbyOptions().mPlayers[j];
            theHost = gWorld.GetLobbyOptions().mPlayers[0];
            LobbyShowPlayer(thePlayer, theHost, 8);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5dcd82
// Copies a loaded game's options (map size, resources, epochs, limits, variant, flags) into the lobby and sends them
long LobbyApplySavedGameOptions(EEPSaveGameHeader& inHeader)
{
    EEGameOptions theOptions(inHeader.mGameOptions);
    gGameOptions.mMapSize = theOptions.mMapSize;
    gGameOptions.mResources = theOptions.mResources;
    gGameOptions.mEpochStart = theOptions.mEpochStart;
    gGameOptions.mEpochEnd = theOptions.mEpochEnd;
    gGameOptions.mUnitLimit = theOptions.mUnitLimit;
    gGameOptions.mWondersForVictory = theOptions.mWondersForVictory;
    gGameOptions.mVariant = theOptions.mVariant;
    gGameOptions.mDifficulty = theOptions.mDifficulty;
    gGameOptions.mGameSpeed = theOptions.mGameSpeed;
    gGameOptions.mRecordGames = theOptions.mRecordGames;
    gGameOptions.mAllowCustomCivs = theOptions.mAllowCustomCivs;
    gGameOptions.mRevealMap = theOptions.mRevealMap;
    gGameOptions.mLockTeams = theOptions.mLockTeams;
    gGameOptions.mLockSpeed = theOptions.mLockSpeed;
    gGameOptions.mCheatCodes = theOptions.mCheatCodes;
    gGameOptions.mVictory = theOptions.mVictory;
    gCommunications->ProcessGuaranteedLocalUserInput(new EEMFullGameOptions(gGameOptions), kSlotNone);
    return 0;
}

// FUNCTION: EXE 0x5dce59
// Redraws part of a slot's lobby row (inFlags: 0 type and name, 1 ping, 2 colour, 3 team, 4 citizens, 5 ready,
// 8 all); also sets the chat prefix to the local player's name
void LobbyShowPlayer(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost, long inFlags)
{
    UWideString theText;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    if (theForm->GetManager()->IsFormVisible(theForm) != true)
        return;
    if (inPlayer.mIsLocal == true && gGameOptions.mIsMultiplayer == true)
    {
        EEUIELEChat* theChat = static_cast<EEUIELEChat*>(theForm->GetControl(kLobbyChatTextEditBox)->GetTriggerEvent(kControlTriggerEnterPressed));
        theChat->mPrefix = inPlayer.mName;
    }
    if (inFlags == 0 || inFlags == 8) {
        if (gCommunications->IsServer() == true)
            LobbyShowHostRow(inPlayer, inHost);
        else
            LobbyShowClientRow(inPlayer, inHost);
        LobbyShowName(inPlayer, inHost);
    }
    if (inFlags == 5 || inFlags == 8) {
        if (inPlayer.mIsLocal == true) {
            if (gGameOptions.mIsMultiplayer == true) {
                theForm->GetControl(inPlayer.mSlot + 12)->SetDisplayMode(inPlayer.mSlot < 8);
                theForm->GetControl(inPlayer.mSlot + 233)->SetDisplayMode(kDisplayModeHidden);
            } else {
                theForm->GetControl(inPlayer.mSlot + 12)->SetDisplayMode(kDisplayModeHidden);
                theForm->GetControl(inPlayer.mSlot + 233)->SetDisplayMode(kDisplayModeHidden);
            }
        } else if (inPlayer.mType == 1 || inPlayer.mType == 2) {
            theForm->GetControl(inPlayer.mSlot + 12)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 233)->SetDisplayMode(inPlayer.mSlot < 8);
        } else {
            theForm->GetControl(inPlayer.mSlot + 12)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 233)->SetDisplayMode(kDisplayModeHidden);
        }
        static_cast<UICheckBox*>(theForm->GetControl(inPlayer.mSlot + 12))->SetChecked(inPlayer.mIsReady);
        static_cast<UIBitmap*>(theForm->GetControl(inPlayer.mSlot + 233))->SetBitmap(
            inPlayer.mIsReady == true ? gUIManager->GetCheckDownOn() : gUIManager->GetCheckDownOff());
    }
    if (inFlags == 2 || inFlags == 8) {
        if (inPlayer.mType == 1) {
            if (inPlayer.mIsLocal == true && inPlayer.mIsReady == false && gGameOptions.mLoadType == 0) {
                theForm->GetControl(inPlayer.mSlot + 78)->SetDisplayMode(inPlayer.mSlot < 8);
                theForm->GetControl(inPlayer.mSlot + 110)->SetDisplayMode(kDisplayModeHidden);
            } else {
                theForm->GetControl(inPlayer.mSlot + 78)->SetDisplayMode(kDisplayModeHidden);
                theForm->GetControl(inPlayer.mSlot + 110)->SetDisplayMode(inPlayer.mSlot < 8);
            }
        } else {
            theForm->GetControl(inPlayer.mSlot + 78)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 110)->SetDisplayMode(kDisplayModeHidden);
        }
        theForm->GetControl(inPlayer.mSlot + 78)->SetBgOpacity(1.0f);
        theForm->GetControl(inPlayer.mSlot + 78)->SetBgColor(EEGetPlayerColor(inPlayer.mColor));
        if (theForm->GetControl(inPlayer.mSlot + 78)->GetTriggerEvent(kControlTriggerRightButtonUp)) {
            static_cast<EEUIELEDecChangeColor*>(theForm->GetControl(inPlayer.mSlot + 78)->GetTriggerEvent(kControlTriggerRightButtonUp))
                ->SetSlot(inPlayer.mColor);
            static_cast<EEUIELEIncChangeColor*>(theForm->GetControl(inPlayer.mSlot + 78)->GetTriggerEvent(kControlTriggerButtonUp))
                ->SetSlot(inPlayer.mColor);
        }
        theForm->GetControl(inPlayer.mSlot + 110)->SetBgOpacity(1.0f);
        theForm->GetControl(inPlayer.mSlot + 110)->SetBgColor(EEGetPlayerColor(inPlayer.mColor));
    }
    if (inFlags == 3 || inFlags == 8) {
        long theLoadType = gGameOptions.mLoadType;
        if (inPlayer.mType != 1 && inPlayer.mType != 0
            && !(inPlayer.mType == 3 && gGameOptions.mLoadType == 1 && inHost.mIsReady == true)) {
            theForm->GetControl(inPlayer.mSlot + 62)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 94)->SetDisplayMode(kDisplayModeHidden);
        } else if (theLoadType == 0 && inPlayer.mIsReady == false
                   && (inPlayer.mIsLocal == true || gCommunications->IsServer() && inPlayer.mType == 0)) {
            theForm->GetControl(inPlayer.mSlot + 62)->SetDisplayMode(inPlayer.mSlot < 8);
            theForm->GetControl(inPlayer.mSlot + 94)->SetDisplayMode(kDisplayModeHidden);
        } else {
            theForm->GetControl(inPlayer.mSlot + 62)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 94)->SetDisplayMode(inPlayer.mSlot < 8);
        }
        if (inPlayer.mTeam == 0)
            theText = "-";
        else
            sprintf(theText, "%d", inPlayer.mTeam);
        UIComboBox* theTeam = static_cast<UIComboBox*>(theForm->GetControl(inPlayer.mSlot + 62));
        unsigned long theSelection;
        if (theTeam->GetCurrentSelection(theSelection) != kErrorOK
            || theSelection != gGameOptions.mPlayers[inPlayer.mSlot].mTeam)
            theTeam->SetCurrentSelection(gGameOptions.mPlayers[inPlayer.mSlot].mTeam);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 94))->SetText(theText);
    }
    if (inFlags == 4 || inFlags == 8) {
        if (inPlayer.mType != 1 && inPlayer.mType != 0
            && !(inPlayer.mType == 3 && gGameOptions.mLoadType == 1 && inHost.mIsReady == true)) {
            theForm->GetControl(inPlayer.mSlot + 126)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 142)->SetDisplayMode(kDisplayModeHidden);
        } else if (gCommunications->IsServer() == true && inHost.mIsReady == false
                   && gGameOptions.mLoadType == 0) {
            theForm->GetControl(inPlayer.mSlot + 126)->SetDisplayMode(inPlayer.mSlot < 8);
            theForm->GetControl(inPlayer.mSlot + 142)->SetDisplayMode(kDisplayModeHidden);
        } else {
            theForm->GetControl(inPlayer.mSlot + 126)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 142)->SetDisplayMode(inPlayer.mSlot < 8);
        }
        static_cast<UIComboBox*>(theForm->GetControl(inPlayer.mSlot + 126))->SetCurrentSelection(inPlayer.mStartingCitizens - 1);
        sprintf(theText, "%d", inPlayer.mStartingCitizens);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 142))->SetText(theText);
    }
    if (inFlags != 1 && inFlags != 8)
        return;
    if ((inPlayer.mType == 1 || inPlayer.mType == 2) && inPlayer.mSlot != 0)
        theForm->GetControl(inPlayer.mSlot + 158)->SetDisplayMode(inPlayer.mSlot < 8);
    else
        theForm->GetControl(inPlayer.mSlot + 158)->SetDisplayMode(kDisplayModeHidden);
    static_cast<EEUICPing*>(theForm->GetControl(inPlayer.mSlot + 158))->SetPing(inPlayer.mPing);
}

// FUNCTION: EXE 0x5dd466
// Shows a slot's type box and name label as a client sees them, and which of its controls are editable
void LobbyShowClientRow(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost)
{
    UWideString theText;
    long theMenu = 1;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    if (gGameOptions.mLoadType == 0 && inPlayer.mIsLocal == true)
        theMenu = 3;
    if (sPlayerTypeMenus[inPlayer.mSlot] != theMenu) {
        LobbyFillPlayerTypes(theMenu, inPlayer, static_cast<UIComboBox*>(theForm->GetControl(inPlayer.mSlot + 30)));
        sPlayerTypeMenus[inPlayer.mSlot] = theMenu;
    }
    if (inPlayer.mType == 0 || inPlayer.mType == 3 && gGameOptions.mLoadType == 1 && inHost.mIsReady == true) {
        gResource->GetString(IDS_COMPUTER, theText);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->SetText(theText);
    } else if (inPlayer.mType == 1 || inPlayer.mType == 2 || inPlayer.mType == 3 && gGameOptions.mLoadType == 2)
        (static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46)))->SetText(inPlayer.mName);
    else if (inPlayer.mType == 3) {
        gResource->GetString(IDS_OPEN, theText);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->SetText(theText);
    } else {
        gResource->GetString(IDS_CLOSED, theText);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->SetText(theText);
    }
    if (inPlayer.mType == 0)
        LobbyShowRowControls(inPlayer.mSlot, 2, 0, 2, 0);
    else if (inPlayer.mType == 1) {
        if (inPlayer.mIsLocal == true) {
            if (gGameOptions.mIsMultiplayer)
                LobbyShowRowControls(inPlayer.mSlot, 1, 1, 1, 0);
            else
                LobbyShowRowControls(inPlayer.mSlot, 1, 1, 1, 1);
        } else
            LobbyShowRowControls(inPlayer.mSlot, 0, 0, 0, 0);
    } else if (inPlayer.mType == 2) {
        if (inPlayer.mIsLocal == true)
            LobbyShowRowControls(inPlayer.mSlot, 1, 2, 2, 2);
        else
            LobbyShowRowControls(inPlayer.mSlot, 0, 2, 2, 2);
    } else
        LobbyShowRowControls(inPlayer.mSlot, 2, 2, 2, 2);
}

// FUNCTION: EXE 0x5dd5b4
// Fills a slot's type box with the entries of the given menu (name, open, computer, closed, observe) and selects one
void LobbyFillPlayerTypes(long inMenu, EEGameOptionsPlayer& inPlayer, UIComboBox* inBox)
{
    UWideString theText;
    inBox->DeleteAllEntries();
    switch (inMenu) {
    case 5:
        inBox->AppendLine(inPlayer.mName);
        gResource->GetString(IDS_OPEN, theText);
        inBox->AppendLine(theText);
        gResource->GetString(IDS_COMPUTER, theText);
        inBox->AppendLine(theText);
        break;
    case 4:
        gResource->GetString(IDS_OPEN, theText);
        inBox->AppendLine(theText);
        gResource->GetString(IDS_COMPUTER, theText);
        inBox->AppendLine(theText);
        break;
    case 3:
        inBox->AppendLine(inPlayer.mName);
        gResource->GetString(IDS_OBSERVE, theText);
        inBox->AppendLine(theText);
        break;
    case 2:
        inBox->AppendLine(inPlayer.mName);
    case 1:
        gResource->GetString(IDS_OPEN, theText);
        inBox->AppendLine(theText);
        gResource->GetString(IDS_COMPUTER, theText);
        inBox->AppendLine(theText);
        gResource->GetString(IDS_CLOSED, theText);
        inBox->AppendLine(theText);
        break;
    }
    LobbySelectPlayerType(inMenu, inPlayer, inBox);
}

// FUNCTION: EXE 0x5dd715
// Selects the type box entry for the slot's type without firing the box's change event
void LobbySelectPlayerType(long inMenu, EEGameOptionsPlayer& inPlayer, UIComboBox* inBox)
{
    UIEvent* theEvent = inBox->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent) {
        theEvent->Reference();
        inBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    switch (inMenu) {
    case 5:
        if (inPlayer.mType == 3)
            inBox->SetCurrentSelection(1);
        else if (inPlayer.mType == 0)
            inBox->SetCurrentSelection(2);
        else
            inBox->SetCurrentSelection(0);
        break;
    case 4:
        if (inPlayer.mType == 3)
            inBox->SetCurrentSelection(0);
        else
            inBox->SetCurrentSelection(1);
        break;
    case 3:
        if (inPlayer.mType == 1)
            inBox->SetCurrentSelection(0);
        else
            inBox->SetCurrentSelection(1);
        break;
    case 2:
        if (inPlayer.mType == 1 || inPlayer.mType == 2)
            inBox->SetCurrentSelection(0);
        else if (inPlayer.mType == 3)
            inBox->SetCurrentSelection(1);
        else if (inPlayer.mType == 0)
            inBox->SetCurrentSelection(2);
        else
            inBox->SetCurrentSelection(3);
        break;
    case 1:
        if (inPlayer.mType == 3)
            inBox->SetCurrentSelection(0);
        else if (inPlayer.mType == 0)
            inBox->SetCurrentSelection(1);
        else
            inBox->SetCurrentSelection(2);
        break;
    }
    if (theEvent) {
        inBox->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
}

// FUNCTION: EXE 0x5dd7cb
// Shows a slot's ready, team, colour and bonus controls: 0 shows the label, 1 the editable control, 2 hides both;
// an editable team box gets a team-change event
void LobbyShowRowControls(unsigned long inSlot, unsigned long inReady, unsigned long inTeam, unsigned long inColor,
                          unsigned long inBonus)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    if (inReady < 2) {
        theForm->GetControl(inSlot + 233)->SetDisplayMode(inReady == 0);
        theForm->GetControl(inSlot + 12)->SetDisplayMode(inReady == 1);
    } else {
        theForm->GetControl(inSlot + 233)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(inSlot + 12)->SetDisplayMode(kDisplayModeHidden);
    }
    if (inTeam < 2) {
        theForm->GetControl(inSlot + 94)->SetDisplayMode(inTeam == 0);
        theForm->GetControl(inSlot + 62)->SetDisplayMode(inTeam == 1);
        if (inTeam == 1) {
            UIEvent* theEvent = new EEUIELEChangeTeam;
            theForm->GetControl(inSlot + 62)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
            theEvent->Dereference();
        } else
            theForm->GetControl(inSlot + 62)->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    } else {
        theForm->GetControl(inSlot + 94)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(inSlot + 62)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(inSlot + 62)->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    if (inColor < 2) {
        theForm->GetControl(inSlot + 110)->SetDisplayMode(inColor == 0);
        theForm->GetControl(inSlot + 78)->SetDisplayMode(inColor == 1);
    } else {
        theForm->GetControl(inSlot + 110)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(inSlot + 78)->SetDisplayMode(kDisplayModeHidden);
    }
    if (inBonus < 2) {
        theForm->GetControl(inSlot + 142)->SetDisplayMode(inBonus == 0);
        theForm->GetControl(inSlot + 126)->SetDisplayMode(inBonus == 1);
    } else {
        theForm->GetControl(inSlot + 142)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(inSlot + 126)->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x5dd9b6
// Shows either the slot's type box or its name label and colours the name (loaded-game, observer or normal slot)
void LobbyShowName(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    if (gGameOptions.mLoadType != 2 && (gCommunications->IsServer() == true || inPlayer.mIsLocal == true)) {
        if ((inPlayer.mIsReady == false && inPlayer.mIsLocal == true && gGameOptions.mIsMultiplayer == true
             || gCommunications->IsServer() == true && inHost.mIsReady == false && inPlayer.mIsLocal == false)
            && (gGameOptions.mLoadType != 1
                || inPlayer.mB36 == false && inPlayer.mIsLocal == false && inPlayer.mType != 4)) {
            theForm->GetControl(inPlayer.mSlot + 30)->SetDisplayMode(inPlayer.mSlot < 8);
            theForm->GetControl(inPlayer.mSlot + 46)->SetDisplayMode(kDisplayModeHidden);
        } else {
            theForm->GetControl(inPlayer.mSlot + 30)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(inPlayer.mSlot + 46)->SetDisplayMode(inPlayer.mSlot < 8);
        }
    } else {
        theForm->GetControl(inPlayer.mSlot + 30)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(inPlayer.mSlot + 46)->SetDisplayMode(inPlayer.mSlot < 8);
    }
    if (inPlayer.mType == 3 && gGameOptions.mLoadType == 2) {
        if (inPlayer.mLoadedType == 1)
            (static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46)))->GetGEText()->SetFgColor(GEColor(1.0f, 0.0f, 0.0f));
        else
            (static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46)))->GetGEText()->SetFgColor(GEColor(1.0f, 1.0f, 0.0f));
    } else {
        if (inPlayer.mType != 2) {
            static_cast<UIComboBox*>(theForm->GetControl(inPlayer.mSlot + 30))->GetTextLabel()->GetGEText()
                ->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
            static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->GetGEText()->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
        } else {
            static_cast<UIComboBox*>(theForm->GetControl(inPlayer.mSlot + 30))->GetTextLabel()->GetGEText()
                ->SetFgColor(GEColor(0.0f, 1.0f, 0.0f));
            static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->GetGEText()->SetFgColor(GEColor(0.0f, 1.0f, 0.0f));
        }
        theForm->GetControl(inPlayer.mSlot + 30)->SetDirty(true);
    }
    theForm->GetControl(inPlayer.mSlot + 46)->SetDirty(true);
}

// FUNCTION: EXE 0x5ddbf1
// Shows a slot's type box and name label as the host sees them, and which of its controls are editable
void LobbyShowHostRow(EEGameOptionsPlayer& inPlayer, const EEGameOptionsPlayer& inHost)
{
    UWideString theText;
    long theMenu = 1;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    if (gGameOptions.mLoadType == 0) {
        if (inPlayer.mIsLocal == true)
            theMenu = 3;
        else if (inPlayer.mType == 1 || inPlayer.mType == 2)
            theMenu = 2;
    } else if (gGameOptions.mLoadType == 1 && inPlayer.mB36 == false) {
        if (inPlayer.mType == 1 || inPlayer.mType == 2)
            theMenu = 5;
        else
            theMenu = 4;
    }
    if (sPlayerTypeMenus[inPlayer.mSlot] != theMenu) {
        LobbyFillPlayerTypes(theMenu, inPlayer, static_cast<UIComboBox*>(theForm->GetControl(inPlayer.mSlot + 30)));
        sPlayerTypeMenus[inPlayer.mSlot] = theMenu;
    }
    if (inPlayer.mType == 0 || inPlayer.mType == 3 && gGameOptions.mLoadType == 1 && inHost.mIsReady == true) {
        gResource->GetString(IDS_COMPUTER, theText);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->SetText(theText);
    } else if (inPlayer.mType == 1 || inPlayer.mType == 2 || inPlayer.mType == 3 && gGameOptions.mLoadType == 2)
        (static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46)))->SetText(inPlayer.mName);
    else if (inPlayer.mType == 3) {
        gResource->GetString(IDS_OPEN, theText);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->SetText(theText);
    } else {
        gResource->GetString(IDS_CLOSED, theText);
        static_cast<UITextLabel*>(theForm->GetControl(inPlayer.mSlot + 46))->SetText(theText);
    }
    if (inPlayer.mType == 0) {
        if (gGameOptions.mLoadType != 1)
            LobbyShowRowControls(inPlayer.mSlot, 2, 1, 2, 1);
        else
            LobbyShowRowControls(inPlayer.mSlot, 2, 0, 2, 0);
    } else if (inPlayer.mType == 1) {
        if (gGameOptions.mLoadType != 1) {
            if (inPlayer.mIsLocal == true)
                LobbyShowRowControls(inPlayer.mSlot, 1, 1, 1, 1);
            else
                LobbyShowRowControls(inPlayer.mSlot, 0, 0, 0, 0);
        } else
            LobbyShowRowControls(inPlayer.mSlot, 2, 0, 2, 0);
    } else if (inPlayer.mType == 2) {
        if (gGameOptions.mLoadType != 1) {
            if (inPlayer.mIsLocal == true)
                LobbyShowRowControls(inPlayer.mSlot, 1, 2, 2, 2);
            else
                LobbyShowRowControls(inPlayer.mSlot, 0, 2, 2, 2);
        } else
            LobbyShowRowControls(inPlayer.mSlot, 2, 0, 2, 0);
    } else
        LobbyShowRowControls(inPlayer.mSlot, 2, 2, 2, 2);
}

// FUNCTION: EXE 0x5ddd8c
// Redraws the given parts of one slot's lobby row from the current game options
void LobbyUpdatePlayer(long inSlot, long inFlags)
{
    EEGameOptionsPlayer thePlayer;
    EEGameOptionsPlayer theHost;
    gGameOptions.mLock->ReadLock();
    thePlayer = gGameOptions.GetPlayer(inSlot);
    theHost = gGameOptions.mPlayers[0];
    LobbyShowPlayer(thePlayer, theHost, inFlags);
    gGameOptions.mLock->ReadUnlock();
}

// FUNCTION: EXE 0x5dde02
// Shows the game options on the lobby: editable boxes for the host before ready (random maps only), labels
// otherwise; record games and victory stay hidden
void LobbyShowGameOptions(const EEGameOptions& inOptions, const EEGameOptionsPlayer& inHost)
{
    UWideString theText;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    bool theReady = inHost.mIsReady;
    bool theServer = gCommunications->IsServer();
    if (theForm != gUIManager->GetCurrentForm())
        return;
    long theType;
    switch (inOptions.GetLoadType()) {
    case 2:
        theType = 1;
        break;
    case 1:
        theType = 2;
        break;
    case 0:
        theType = 0;
        break;
    default:
        theType = 0;
        break;
    }
    unsigned long theSelection;
    if (theServer
        && (static_cast<UIComboBox*>(theForm->GetControl(kLobbyGameTypeCombo))->GetCurrentSelection(theSelection) != kErrorOK
            || theSelection != theType))
        (static_cast<UIComboBox*>(theForm->GetControl(kLobbyGameTypeCombo)))->SetCurrentSelection(theType);
    switch (inOptions.GetLoadType()) {
    case 2:
        gResource->GetString(sGameTypeStrings[1], theText);
        break;
    case 1:
        gResource->GetString(sGameTypeStrings[2], theText);
        break;
    case 0:
        gResource->GetString(sGameTypeStrings[0], theText);
        break;
    }
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameTypeLabel2))->SetText(theText);
    if (theServer && !theReady && gGameOptions.mIsMultiplayer == true) {
        theForm->GetControl(kLobbyGameTypeCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameTypeLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameTypeCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameTypeLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyMapSizeCombo))->SetCurrentSelection(inOptions.GetMapSize());
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyMapSizeCombo))->GetCurrentSelection(theSelection);
    gResource->GetString(IDS_MAP_SIZE_FIRST + theSelection, theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyMapSizeLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0) {
        theForm->GetControl(kLobbyMapSizeLabel0)->SetDisplayMode(kDisplayModeShown);
        if (theServer && !theReady) {
            theForm->GetControl(kLobbyMapSizeCombo)->SetDisplayMode(kDisplayModeShown);
            theForm->GetControl(kLobbyMapSizeLabel2)->SetDisplayMode(kDisplayModeHidden);
        } else {
            theForm->GetControl(kLobbyMapSizeCombo)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(kLobbyMapSizeLabel2)->SetDisplayMode(kDisplayModeShown);
        }
    } else {
        theForm->GetControl(kLobbyMapSizeLabel0)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyMapSizeCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyMapSizeLabel2)->SetDisplayMode(kDisplayModeHidden);
    }
    if (EERMFindGenerator(inOptions.GetMapType(), theSelection))
        theSelection = 0;
    if (gGameOptions.GetLoadType() == 0)
        (static_cast<UIComboBox*>(theForm->GetControl(kLobbyMapTypeCombo)))->SetCurrentSelection(theSelection);
    theText = EERMGetGeneratorDisplayName(inOptions.GetMapType());
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyMapTypeLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0) {
        theForm->GetControl(kLobbyMapTypeLabel0)->SetDisplayMode(kDisplayModeShown);
        if (theServer && !theReady) {
            theForm->GetControl(kLobbyMapTypeCombo)->SetDisplayMode(kDisplayModeShown);
            theForm->GetControl(kLobbyMapTypeLabel2)->SetDisplayMode(kDisplayModeHidden);
        } else {
            theForm->GetControl(kLobbyMapTypeCombo)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(kLobbyMapTypeLabel2)->SetDisplayMode(kDisplayModeShown);
        }
    } else {
        theForm->GetControl(kLobbyMapTypeLabel0)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyMapTypeCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyMapTypeLabel2)->SetDisplayMode(kDisplayModeHidden);
    }
    if (inOptions.GetLoadType() == 0) {
        theForm->GetControl(kLobbyScenarioInfoMLText)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyScenarioInfoLabel)->SetDisplayMode(kDisplayModeHidden);
    } else {
        static_cast<UITextLabel*>(theForm->GetControl(kLobbyScenarioInfoMLText))->SetText(gGameOptions.GetMapType());
        theForm->GetControl(kLobbyScenarioInfoMLText)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyScenarioInfoLabel)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyResourcesCombo))->SetCurrentSelection(inOptions.GetResources());
    gResource->GetString(EEStartingResources::Get(inOptions.GetResources())->mNameID, theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyResourcesLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyResourcesCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyResourcesLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyResourcesCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyResourcesLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    gResource->GetString(IDS_EPOCH_NAME_FIRST + inOptions.GetEpochStart(), theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyEpochStartLabel2))->SetText(theText);
    if (static_cast<UIComboBox*>(theForm->GetControl(kLobbyEpochStartCombo))->GetCurrentSelection(theSelection) != kErrorOK)
        (static_cast<UIComboBox*>(theForm->GetControl(kLobbyEpochStartCombo)))->SetCurrentSelection(inOptions.GetEpochStart());
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyEpochStartCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyEpochStartLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyEpochStartCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyEpochStartLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    gResource->GetString(IDS_EPOCH_NAME_FIRST + inOptions.GetEpochEnd(), theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyEpochEndLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyEpochEndCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyEpochEndLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyEpochEndCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyEpochEndLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyWorldUnitLimitCombo))->SetCurrentSelection(
        (inOptions.GetUnitLimit() - gWorldTable.GetLongValue(26)) / 50);
    sprintf(theText, "%d", inOptions.GetUnitLimit());
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyWorldPopLimitLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyWorldUnitLimitCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyWorldPopLimitLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyWorldUnitLimitCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyWorldPopLimitLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyWondersVictoryCombo))->SetCurrentSelection(inOptions.GetWondersForVictory());
    if (inOptions.GetWondersForVictory() != 0)
        sprintf(theText, "%d", inOptions.GetWondersForVictory());
    else
        gResource->GetString(IDS_LOBBY_OFF, theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyWondersVictoryLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyWondersVictoryCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyWondersVictoryLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyWondersVictoryCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyWondersVictoryLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    unsigned long theVariant = 0;
    if (EEGetGameVariantAt(inOptions.GetVariant())->mSelectable == true)
        for (unsigned long i = 0; i < inOptions.GetVariant(); i++)
            if (EEGetGameVariantAt(i)->mSelectable == true)
                theVariant++;
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyGameVariantCombo))->SetCurrentSelection(theVariant);
    gResource->GetString(EEGetGameVariantAt(inOptions.GetVariant())->mNameID, theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameVariantLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyGameVariantCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameVariantLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameVariantCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameVariantLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyDifficultyCombo))->SetCurrentSelection(inOptions.GetDifficulty());
    gResource->GetString(sDifficultyStrings[inOptions.GetDifficulty()], theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyDifficultyLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyDifficultyCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyDifficultyLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyDifficultyCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyDifficultyLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UIComboBox*>(theForm->GetControl(kLobbyGameSpeedCombo))->SetCurrentSelection(inOptions.GetGameSpeed());
    gResource->GetString(sGameSpeedStrings[inOptions.GetGameSpeed()], theText);
    static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameSpeedLabel2))->SetText(theText);
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyGameSpeedCombo)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameSpeedLabel2)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameSpeedCombo)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameSpeedLabel2)->SetDisplayMode(kDisplayModeShown);
    }
    if (inOptions.GetRecordGames() != static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox6RecGame))->GetIsChecked())
        (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox6RecGame)))->SetChecked(inOptions.GetRecordGames());
    if (inOptions.GetRecordGames() == true)
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox6Bitmap)))->SetBitmap(gUIManager->GetCheckDownOn());
    else
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox6Bitmap)))->SetBitmap(gUIManager->GetCheckDownOff());
    if (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox6RecGame))->GetTextLabel())
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox6Label)))->SetText(
            static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox6RecGame))->GetTextLabel()->GetText(0));
    else
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox6Label)))->SetText("");
    theForm->GetControl(kLobbyGameOpCheckBox6RecGame)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControl(kLobbyGameOpCheckBox6Bitmap)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControl(kLobbyGameOpCheckBox6Label)->SetDisplayMode(kDisplayModeHidden);
    if (inOptions.GetAllowCustomCivs() != static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox2CustomCiv))->GetIsChecked())
        (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox2CustomCiv)))->SetChecked(inOptions.GetAllowCustomCivs());
    if (inOptions.GetAllowCustomCivs() == true)
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox2Bitmap)))->SetBitmap(gUIManager->GetCheckDownOn());
    else
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox2Bitmap)))->SetBitmap(gUIManager->GetCheckDownOff());
    if (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox2CustomCiv))->GetTextLabel())
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox2Label)))->SetText(
            static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox2CustomCiv))->GetTextLabel()->GetText(0));
    else
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox2Label)))->SetText("");
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyGameOpCheckBox2CustomCiv)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox2Bitmap)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox2Label)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameOpCheckBox2CustomCiv)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox2Bitmap)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox2Label)->SetDisplayMode(kDisplayModeShown);
    }
    if (inOptions.GetRevealMap() != static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox1RevealMap))->GetIsChecked())
        (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox1RevealMap)))->SetChecked(inOptions.GetRevealMap());
    if (inOptions.GetRevealMap() == true)
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox1Bitmap)))->SetBitmap(gUIManager->GetCheckDownOn());
    else
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox1Bitmap)))->SetBitmap(gUIManager->GetCheckDownOff());
    if (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox1RevealMap))->GetTextLabel())
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox1Label)))->SetText(
            static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox1RevealMap))->GetTextLabel()->GetText(0));
    else
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox1Label)))->SetText("");
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyGameOpCheckBox1RevealMap)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox1Bitmap)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox1Label)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameOpCheckBox1RevealMap)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox1Bitmap)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox1Label)->SetDisplayMode(kDisplayModeShown);
    }
    if (inOptions.GetLockTeams() != static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox3LockTeam))->GetIsChecked())
        (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox3LockTeam)))->SetChecked(inOptions.GetLockTeams());
    if (inOptions.GetLockTeams() == true)
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox3Bitmap)))->SetBitmap(gUIManager->GetCheckDownOn());
    else
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox3Bitmap)))->SetBitmap(gUIManager->GetCheckDownOff());
    if (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox3LockTeam))->GetTextLabel())
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox3Label)))->SetText(
            static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox3LockTeam))->GetTextLabel()->GetText(0));
    else
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox3Label)))->SetText("");
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyGameOpCheckBox3LockTeam)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox3Bitmap)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox3Label)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameOpCheckBox3LockTeam)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox3Bitmap)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox3Label)->SetDisplayMode(kDisplayModeShown);
    }
    if (inOptions.GetLockSpeed() != static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox4LockSpeed))->GetIsChecked())
        (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox4LockSpeed)))->SetChecked(inOptions.GetLockSpeed());
    if (inOptions.GetLockSpeed() == true)
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox4Bitmap)))->SetBitmap(gUIManager->GetCheckDownOn());
    else
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox4Bitmap)))->SetBitmap(gUIManager->GetCheckDownOff());
    if (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox4LockSpeed))->GetTextLabel())
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox4Label)))->SetText(
            static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox4LockSpeed))->GetTextLabel()->GetText(0));
    else
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox4Label)))->SetText("");
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        theForm->GetControl(kLobbyGameOpCheckBox4LockSpeed)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox4Bitmap)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox4Label)->SetDisplayMode(kDisplayModeHidden);
    } else {
        theForm->GetControl(kLobbyGameOpCheckBox4LockSpeed)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox4Bitmap)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox4Label)->SetDisplayMode(kDisplayModeShown);
    }
    if (inOptions.GetCheatCodes() != static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox5Cheats))->GetIsChecked())
        (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox5Cheats)))->SetChecked(inOptions.GetCheatCodes());
    if (inOptions.GetCheatCodes() == true)
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox5Bitmap)))->SetBitmap(gUIManager->GetCheckDownOn());
    else
        (static_cast<UIBitmap*>(theForm->GetControl(kLobbyGameOpCheckBox5Bitmap)))->SetBitmap(gUIManager->GetCheckDownOff());
    if (static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox5Cheats))->GetTextLabel())
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox5Label)))->SetText(
            static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox5Cheats))->GetTextLabel()->GetText(0));
    else
        (static_cast<UITextLabel*>(theForm->GetControl(kLobbyGameOpCheckBox5Label)))->SetText("");
    if (inOptions.GetLoadType() == 0 && theServer && !theReady) {
        if (inOptions.mIsMultiplayer == false) {
            theForm->GetControl(kLobbyGameOpCheckBox5Cheats)->SetDisplayMode(kDisplayModeShown);
            theForm->GetControl(kLobbyGameOpCheckBox5Bitmap)->SetDisplayMode(kDisplayModeHidden);
            theForm->GetControl(kLobbyGameOpCheckBox5Label)->SetDisplayMode(kDisplayModeHidden);
        }
    } else if (inOptions.mIsMultiplayer == false) {
        theForm->GetControl(kLobbyGameOpCheckBox5Cheats)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControl(kLobbyGameOpCheckBox5Bitmap)->SetDisplayMode(kDisplayModeShown);
        theForm->GetControl(kLobbyGameOpCheckBox5Label)->SetDisplayMode(kDisplayModeShown);
    }
    static_cast<UICheckBox*>(theForm->GetControl(kLobbyGameOpCheckBox7Victory))->SetChecked(true);
    theForm->GetControl(kLobbyGameOpCheckBox7Victory)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControl(kLobbyGameOpCheckBox7Bitmap)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControl(kLobbyGameOpCheckBox7Label)->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x5deefb
// Redraws the lobby's game options from the current options
void LobbyUpdateGameOptions()
{
    EEGameOptionsPlayer theHost;
    gGameOptions.mLock->ReadLock();
    theHost = gGameOptions.mPlayers[0];
    LobbyShowGameOptions(gGameOptions, theHost);
    gGameOptions.mLock->ReadUnlock();
}

// FUNCTION: EXE 0x5def48
// Redraws every slot and the game options, then refreshes the lobby form
void LobbyRefresh()
{
    EEGameOptionsPlayer thePlayer;
    EEGameOptionsPlayer theHost;
    gGameOptions.mLock->ReadLock();
    theHost = gGameOptions.mPlayers[0];
    for (unsigned char i = 0; i < 16; i++) {
        thePlayer = gGameOptions.mPlayers[i];
        LobbyShowPlayer(thePlayer, theHost, 8);
    }
    LobbyShowGameOptions(gGameOptions, theHost);
    gGameOptions.mLock->ReadUnlock();
    LobbyUpdateForm();
}

// FUNCTION: EXE 0x5defd3
// Trims or refills the end-epoch box so it only offers epochs from the chosen start epoch on
void LobbyUpdateEpochEnd()
{
    UWideString theText;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    UIComboBox* theStart = static_cast<UIComboBox*>(theForm->GetControl(kLobbyEpochStartCombo));
    unsigned long theSelection;
    if (theStart->GetCurrentSelection(theSelection) == kErrorOK) {
        UIComboBox* theEnd = static_cast<UIComboBox*>(theStart->GetForm()->GetControl(185));
        long theFirst = theSelection == 15 ? 0 : theSelection;
        long theLimit = 15 - theEnd->GetListBox()->GetLineCount();
        if (theFirst > theLimit) {
            for (long i = theLimit; i < theFirst; i++)
                theEnd->DeleteLine(0);
        } else if (theFirst < theLimit) {
            for (long i = theLimit - 1; i >= theFirst; i--) {
                gResource->GetString(IDS_EPOCH_NAME_FIRST + i, theText);
                theEnd->InsertLine(theText, 0);
            }
        }
    }
}

// FUNCTION: EXE 0x5df093
// Shows a client's file transfer percentage on its row; at 0 or 100 hides it, and at 100 the host clears its transfer
void LobbyShowTransferProgress(const unsigned char& inSlot, const unsigned char& inProgress)
{
    EEGameOptionsPlayer thePlayer;
    UIForm* theForm = gUIManager->GetFormByID(kFormLobby);
    UWideString theText;
    if (gUIManager->IsFormVisible(theForm) == true && inSlot > 0 && inSlot < 16) {
        if (inProgress != 100 && inProgress != 0) {
            if (theForm == gUIManager->GetCurrentForm()) {
                UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(inSlot + 256));
                sprintf(theText, "%d", inProgress);
                theLabel->SetText(theText);
                theLabel->SetDisplayMode(kDisplayModeShown);
            }
        } else {
            theForm->GetControl(inSlot + 256)->SetDisplayMode(kDisplayModeHidden);
            if (inProgress == 100 && gCommunications->IsServer() == true) {
                gGameOptions.mLock->WriteLock();
                thePlayer = gGameOptions.mPlayers[inSlot];
                thePlayer.mTransferID = 0;
                gGameOptions.mPlayers[inSlot] = thePlayer;
                gGameOptions.mLock->WriteUnlock();
            }
        }
    }
}

// FUNCTION: EXE 0x5df1cc
// Opens the lobby as a multiplayer host: fills the boxes, hooks up Cancel, Start and chat, shows Start, starts pinging
long EEUIELEActivateMultiPlayerServer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetEntityGroup(1)->Disable();
    LobbyOpen(inForm);
    UIEvent* theEvent = new EEUIELECancel;
    inForm->GetControl(kLobbyCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEStart;
    inForm->GetControl(kLobbyStartGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    UIEvent* theChatEvent = new EEUIELEChat;
    inForm->GetControl(kLobbyChatTextEditBox)->SetTriggerEvent(kControlTriggerEnterPressed, theChatEvent);
    theChatEvent->Dereference();
    UIEvent* theFocusEvent = new EEUIELEChatKeyboardFocus;
    inForm->SetFormKeyboardCapturedEvent(theFocusEvent);
    theFocusEvent->Dereference();
    inForm->GetControl(kLobbyStartGameButton)->SetDisplayMode(kDisplayModeShown);
    static_cast<EEServer*>(gCommunications)->StartPing();
    inForm->SetTabStop(9);
    inForm->GetBackgroundByID(0x89)->SetVisibility(true);
    inForm->GetBackgroundByID(0xa2)->SetVisibility(false);
    inForm->GetControl(kLobbyGameOpCheckBox5Cheats)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyGameOpCheckBox5Label)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyGameOpCheckBox5Bitmap)->SetDisplayMode(kDisplayModeHidden);
    UIBlinkingButton* theButton = static_cast<UIBlinkingButton*>(inForm->GetControl(kLobbyLagIndicator));
    theButton->SetIsBlinking(true);
    theButton->SetDisplayMode(kDisplayModeHidden);
    return kErrorOK;
}

// FUNCTION: EXE 0x5df38e
// Moves the focus to the chat box when no control holds the keyboard
// (EEUIELEChatKeyboardFocus's inline ProcessEvent, then the ??_G of ChatKeyboardFocus, Cancel, Chat (with Chat's
// implicit ctor) and Start in declaration order)

// FUNCTION: EXE 0x5df3a8
// Destroys the chat-focus event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5df3d3
// Destroys the lobby cancel event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5df3fe
// Creates the chat event with an empty name prefix

// FUNCTION: EXE 0x5df42d
// Destroys the chat event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5df461
// Destroys the start event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5df48c
// Fills the lobby's combo boxes (bonus, team, map size and type, resources, epochs, unit limit, wonders, variants,
// difficulty, speed, game type) and forgets the slots' type menus
void LobbyOpen(UIForm* inForm)
{
    UWideString theText;
    long i;
    unsigned long j;
    for (i = 126; i < 142; i++)
        for (j = 1; j <= 20; j++) {
            sprintf(theText, "%d", j);
            static_cast<UIComboBox*>(inForm->GetControl(i))->AppendLine(theText);
        }
    for (i = 62; i < 78; i++) {
        static_cast<UIComboBox*>(inForm->GetControl(i))->AppendLine(UWideString("-"));
        for (j = 1; j <= 4; j++) {
            sprintf(theText, "%d", j);
            static_cast<UIComboBox*>(inForm->GetControl(i))->AppendLine(theText);
        }
    }
    for (j = IDS_MAP_SIZE_FIRST; j <= IDS_GIGANTIC; j++) {
        gResource->GetString(j, theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyMapSizeCombo))->AppendLine(theText);
    }
    unsigned long theCount = EERMGetNumberOfGenerators();
    for (j = 0; j < theCount; j++)
        (static_cast<UIComboBox*>(inForm->GetControl(kLobbyMapTypeCombo)))->AppendLine(UWideString((const char*)EERMGetGeneratorDisplayName(j)));
    for (j = 0; j < EEGetNumberOfResourceLevels(); j++) {
        gResource->GetString(EEGetResourceLevel(j)->GetNameID(), theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyResourcesCombo))->AppendLine(theText);
    }
    for (j = 0; j <= 15; j++) {
        gResource->GetString(IDS_EPOCH_NAME_FIRST + j, theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyEpochStartCombo))->AppendLine(theText);
    }
    for (j = 0; j < 15; j++) {
        gResource->GetString(IDS_EPOCH_NAME_FIRST + j, theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyEpochEndCombo))->AppendLine(theText);
    }
    for (j = gWorldTable.GetLongValue(26); j <= gWorldTable.GetLongValue2(26); j += 50) {
        sprintf(theText, "%d", j);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyWorldUnitLimitCombo))->AppendLine(theText);
    }
    gResource->GetString(IDS_LOBBY_OFF, theText);
    static_cast<UIComboBox*>(inForm->GetControl(kLobbyWondersVictoryCombo))->AppendLine(theText);
    for (j = 1; j <= gWorldTable.GetLongValue(77); j++) {
        sprintf(theText, "%d", j);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyWondersVictoryCombo))->AppendLine(theText);
    }
    for (j = 0; j < EEGetNumberOfGameVariants(); j++)
        if (gGameVariants[j]->mSelectable == true) {
            gResource->GetString(gGameVariants[j]->mNameID, theText);
            static_cast<UIComboBox*>(inForm->GetControl(kLobbyGameVariantCombo))->AppendLine(theText);
        }
    for (i = 0; i < 3; i++) {
        gResource->GetString(sDifficultyStrings[i], theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyDifficultyCombo))->AppendLine(theText);
    }
    for (i = 0; i < 4; i++) {
        gResource->GetString(sGameSpeedStrings[i], theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyGameSpeedCombo))->AppendLine(theText);
    }
    for (i = 0; i < 3; i++) {
        gResource->GetString(sGameTypeStrings[i], theText);
        static_cast<UIComboBox*>(inForm->GetControl(kLobbyGameTypeCombo))->AppendLine(theText);
    }
    for (i = 0; i < 16; i++)
        sPlayerTypeMenus[i] = 0;
    EE_LOST_DEBUG(4);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x5df874
// Opens the lobby as a multiplayer client: fills the boxes, hooks up Cancel and chat, hides Start, leaves the WON room
long EEUIELEActivateMultiPlayerClient::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetEntityGroup(1)->Disable();
    LobbyOpen(inForm);
    UIEvent* theEvent = new EEUIELECancel;
    inForm->GetControl(kLobbyCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEChat;
    inForm->GetControl(kLobbyChatTextEditBox)->SetTriggerEvent(kControlTriggerEnterPressed, theEvent);
    theEvent->Dereference();
    UIEvent* theFocusEvent = new EEUIELEChatKeyboardFocus;
    inForm->SetFormKeyboardCapturedEvent(theFocusEvent);
    theFocusEvent->Dereference();
    inForm->GetControl(kLobbyStartGameButton)->SetDisplayMode(kDisplayModeHidden);
    inForm->SetTabStop(9);
    inForm->GetBackgroundByID(0x89)->SetVisibility(true);
    inForm->GetBackgroundByID(0xa2)->SetVisibility(false);
    inForm->GetControl(kLobbyGameOpCheckBox5Cheats)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyGameOpCheckBox5Label)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyGameOpCheckBox5Bitmap)->SetDisplayMode(kDisplayModeHidden);
    UIBlinkingButton* theButton = static_cast<UIBlinkingButton*>(inForm->GetControl(kLobbyLagIndicator));
    theButton->SetIsBlinking(true);
    theButton->SetDisplayMode(kDisplayModeHidden);
    if (gWonNet)
        gWonNet->LeaveRoom();
    return kErrorOK;
}

// FUNCTION: EXE 0x5df9e8
// Opens the lobby for single player: fills the boxes, hooks up Cancel and Start and hides the chat, ready and ping
long EEUIELEActivateSinglePlayer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetEntityGroup(1)->Disable();
    LobbyOpen(inForm);
    UIEvent* theEvent = new EEUIELECancel;
    inForm->GetControl(kLobbyCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIELEStart;
    inForm->GetControl(kLobbyStartGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->GetControl(kLobbyChatTextEditBox)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyChatPreviousMessages)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyReadyLabel)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kLobbyPingLabel)->SetDisplayMode(kDisplayModeHidden);
    inForm->SetFormKeyboardCapturedEvent(NULL);
    inForm->GetControl(kLobbyStartGameButton)->SetDisplayMode(kDisplayModeShown);
    inForm->GetBackgroundByID(0x89)->SetVisibility(false);
    inForm->GetBackgroundByID(0xa2)->SetVisibility(true);
    UIBlinkingButton* theButton = static_cast<UIBlinkingButton*>(inForm->GetControl(kLobbyLagIndicator));
    theButton->SetIsBlinking(true);
    theButton->SetDisplayMode(kDisplayModeHidden);
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfb38
// Sets the reveal-map option from its check box and sends the change
long EEUIELEGameRevealMap::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mRevealMap) {
        NEMessage* theMessage = new EEMGameOptions(13, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mRevealMap = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfbe8
// Sets the allow-custom-civs option from its check box and sends the change
long EEUIELEGameTeamIslands::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mAllowCustomCivs) {
        NEMessage* theMessage = new EEMGameOptions(12, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mAllowCustomCivs = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfc98
// Sets the record-games option from its check box and sends the change
long EEUIELEGameRecordGames::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mRecordGames) {
        NEMessage* theMessage = new EEMGameOptions(11, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mRecordGames = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfd48
// Sets the lock-teams option from its check box and sends the change
long EEUIELEGameLockTeams::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mLockTeams) {
        NEMessage* theMessage = new EEMGameOptions(14, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mLockTeams = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfdf8
// Sets the cheat-codes option from its check box and sends the change
long EEUIELEGameCheatCodes::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mCheatCodes) {
        NEMessage* theMessage = new EEMGameOptions(16, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mCheatCodes = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfea8
// Sets the lock-speed option from its check box and sends the change
long EEUIELEGameLockSpeed::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gGameOptions.mLock->WriteLock();
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    if (theBox->GetIsChecked() != gGameOptions.mLockSpeed) {
        NEMessage* theMessage = new EEMGameOptions(15, theBox->GetIsChecked() == true, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mLockSpeed = theBox->GetIsChecked() == true;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dff58
// Sets the map size from its box and sends the change
long EEUIELEGameMapSize::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    gGameOptions.mLock->WriteLock();
    if (theSelection != gGameOptions.mMapSize) {
        NEMessage* theMessage = new EEMGameOptions(1, theSelection, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mMapSize = theSelection;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5dfff4
// Handles the game type box: scenario or saved game opens the load screen, random map clears the loaded game, sends
// it and redraws the lobby
long EEUIELEGameType::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEGameOptionsPlayer thePlayer;
    EEGameOptionsPlayer theHost;
    EEGameOptions theOptions;
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        gGameOptions.mLock->WriteLock();
        switch (theSelection) {
        case 2:
            inForm->GetManager()->GetFormByID(kFormLoadGame)->SetFormOpenEvent(new EEUIELEOpenLoadScenarioGame);
            inForm->GetManager()->Push(inForm->GetManager()->GetFormByID(kFormLoadGame));
            static_cast<EEUIELECancelLoad*>(inForm->GetManager()->GetCurrentForm()->GetControl(2)->GetTriggerEvent(kControlTriggerButtonUp))
                ->SetLoadType(gGameOptions.GetLoadType());
            break;
        case 1:
            inForm->GetManager()->GetFormByID(kFormLoadGame)->SetFormOpenEvent(new EEUIELEOpenLoadSavedGame);
            inForm->GetManager()->Push(inForm->GetManager()->GetFormByID(kFormLoadGame));
            static_cast<EEUIELECancelLoad*>(inForm->GetManager()->GetCurrentForm()->GetControl(2)->GetTriggerEvent(kControlTriggerButtonUp))
                ->SetLoadType(gGameOptions.GetLoadType());
            break;
        case 0:
            if (gGameOptions.mLoadType != 0) {
                NEMessage* theMessage = new EEMGameOptions(0, 0, UWideString(""));
                gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
                EEGameOptions& theGameOptions = gGameOptions;
                theOptions = theGameOptions;
                theOptions.mLoadType = 0;
                theGameOptions = theOptions;
                theHost = theGameOptions.mPlayers[0];
                LobbyShowGameOptions(theOptions, theHost);
                for (unsigned char i = 0; i < 16; i++) {
                    thePlayer = theGameOptions.mPlayers[i];
                    LobbyShowPlayer(thePlayer, theHost, 8);
                }
            }
            break;
        }
        gGameOptions.mLock->WriteUnlock();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e01f3
// Destroys the open-load-saved-game event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e021e
// Destroys the open-load-scenario event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e0249
// Sets the game speed from its box and sends the change
long EEUIELEGameSpeed::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    gGameOptions.mLock->WriteLock();
    if (theSelection != gGameOptions.mGameSpeed) {
        NEMessage* theMessage = new EEMGameOptions(10, theSelection, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mGameSpeed = theSelection;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e02e5
// Sets the difficulty from its box (also the global difficulty levels) and sends the change
long EEUIELEGameDifficulty::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    gGameOptions.mLock->WriteLock();
    if (theSelection != gGameOptions.mDifficulty) {
        gDifficultyLevel = theSelection;
        gDifficultyLevel2 = theSelection;
        NEMessage* theMessage = new EEMGameOptions(9, theSelection, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mDifficulty = theSelection;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e0391
// Sets the game variant from its box (the n-th selectable variant) and sends the change
long EEUIELEGameVariant::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        long theVariant = 0;
        unsigned long theCount = 0;
        gGameOptions.mLock->WriteLock();
        for (;; theVariant++) {
            if (EEGameVariant::GetGameVariant(theVariant)->mSelectable != true)
                continue;
            if (theCount == theSelection)
                break;
            theCount++;
        }
        if (theVariant != gGameOptions.mVariant) {
            NEMessage* theMessage = new EEMGameOptions(8, theVariant, UWideString(""));
            gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
            gGameOptions.mVariant = theVariant;
        }
        gGameOptions.mLock->WriteUnlock();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e044e
// Sets the number of wonders for victory from its box and sends the change
long EEUIELEGameWondersForVictory::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    gGameOptions.mLock->WriteLock();
    if (theSelection != gGameOptions.mWondersForVictory) {
        NEMessage* theMessage = new EEMGameOptions(7, theSelection, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mWondersForVictory = theSelection;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e04ea
// Sets the unit limit from its box (minimum plus 50 per entry) and sends the change
long EEUIELEGameUnitLimit::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    gGameOptions.mLock->WriteLock();
    if (theSelection != (gGameOptions.mUnitLimit - gWorldTable.GetLongValue(26)) / 50) {
        NEMessage* theMessage = new EEMGameOptions(6, theSelection * 50 + gWorldTable.GetLongValue(26),
                                                   UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mUnitLimit = theSelection * 50 + gWorldTable.GetLongValue(26);
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e05be
// Sets the end epoch from its box (counted from the start epoch) and sends the change
long EEUIELEEpochEnd::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    gGameOptions.mLock->WriteLock();
    if (gGameOptions.mEpochStart != kTechEpochAll)
        theSelection += gGameOptions.mEpochStart;
    if (theSelection != gGameOptions.mEpochEnd) {
        NEMessage* theMessage = new EEMGameOptions(5, theSelection, UWideString(""));
        gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
        gGameOptions.mEpochEnd = theSelection;
    }
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e066a
// Sets the start epoch from its box, trims the end-epoch box and moves the end epoch up when it lies before the start
long EEUIELEGameEpochStart::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        gGameOptions.mLock->WriteLock();
        if (theSelection != gGameOptions.mEpochStart) {
            LobbyUpdateEpochEnd();
            NEMessage* theMessage = new EEMGameOptions(4, theSelection, UWideString(""));
            gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
            gGameOptions.mEpochStart = theSelection;
            if (theSelection >= 15)
                theSelection = 0;
            if (gGameOptions.mEpochEnd < theSelection) {
                theMessage = new EEMGameOptions(5, theSelection, UWideString(""));
                gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
                gGameOptions.mEpochEnd = theSelection;
            }
        }
        gGameOptions.mLock->WriteUnlock();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e0783
// Sets the starting resources from its box and sends the change
long EEUIELEGameResources::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        gGameOptions.mLock->WriteLock();
        if (theSelection != gGameOptions.mResources) {
            NEMessage* theMessage = new EEMGameOptions(3, theSelection, UWideString(""));
            gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
            gGameOptions.mResources = theSelection;
        }
        gGameOptions.mLock->WriteUnlock();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e0831
// Sets the random map type from its box and sends the change
long EEUIELEGameMapType::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    UIComboBox* theBox = static_cast<UIComboBox*>(inEntity);
    unsigned long theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK) {
        gGameOptions.mLock->WriteLock();
        theName = EERMGetGeneratorName(theSelection);
        UWideString& theMapType = gGameOptions.mMapType;
        if (theName != theMapType) {
            NEMessage* theMessage = new EEMGameOptions(2, theSelection, theName);
            gCommunications->ProcessGuaranteedLocalUserInput(theMessage, kSlotNone);
            theMapType = theName;
        }
        gGameOptions.mLock->WriteUnlock();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e08db
// Returns the last slot of a vector of lobby slots
// (vector<EEGameOptionsPlayer> template COMDATs, emitted for LobbyRebuildPlayers)

// FUNCTION: EXE 0x5e08e2
// Appends a slot to a vector of lobby slots

// FUNCTION: EXE 0x5e090e
// Removes the last slot of a vector of lobby slots

// FUNCTION: EXE 0x5e0942
// Erases one slot from a vector of lobby slots; returns the position after it

// FUNCTION: EXE 0x5e09c3
// Inserts one slot into a vector of lobby slots; returns its position

// FUNCTION: EXE 0x5e0a1a
// Destroys a vector of lobby slots' elements and frees its storage

// FUNCTION: EXE 0x5e0a5c
// Inserts n copies of a slot into a vector of lobby slots, growing it when needed

// FUNCTION: EXE 0x5e0d40
// Copy-constructs n copies of a slot into raw vector storage

// FUNCTION: EXE 0x5e0d6b
// Copies a range of lobby slots forward

// FUNCTION: EXE 0x5e0d94
// Copy-constructs a range of lobby slots into raw vector storage

// FUNCTION: EXE 0x5e0dc0
// Assigns one slot value to a range of lobby slots

// FUNCTION: EXE 0x5e0ddd
// Copies a range of lobby slots backward

// FUNCTION: EXE 0x5e0e09
// Copy-constructs one lobby slot in place
