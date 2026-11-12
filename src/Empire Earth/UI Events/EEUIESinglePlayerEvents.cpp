// C:\EEX\Empire Earth\UI Events\EEUIESinglePlayerEvents.cpp (file name inferred: no __FILE__; class prefix EEUIESPE).
// The single-player screen's events: 0x1e8a24..0x1e8e7d. Unit flags: the exe module set.
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

#include "lle/NESynchronousServer.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEUIELobbyEvents.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "rust/ee_rust.h"   // ee_screens_single_player_slots (ee_screens single_player.rs)
#include "ee/EEUIEPlayCampaignEvents.h"
#include "ee/EEUIESinglePlayerEvents.h"
#include "ee/EEStringIDs.h"
void SPESavePlayerName(UIForm* inForm);

// FUNCTION: EXE 0x5e8a24
// Fills the single-player screen's name box with the saved player name
long EEUIESPEOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static_cast<UIEditBox*>(inForm->GetControl(kSinglePlayerPlayerNameEditBox))->SetText(gPlayerName);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8a41
// Saves the typed player name and switches to the play-campaign screen, hooking up its open event
long EEUIESPECampaign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    SPESavePlayerName(inForm);
    UIForm* theForm = gUIManager->GetFormByID(kFormPlayCampaign);
    UIEvent* theEvent = new EEUIEPCEFormOpen;
    theForm->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    gUserInterface->SwitchToForm(theForm);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8aa1
// Destroys the play-campaign open event and frees it when asked (scalar deleting destructor)
// (EEUIEPCEFormOpen's ??_G, after its first user)

// FUNCTION: EXE 0x5e8acc
// Stores the name typed on the single-player screen as the player name, unless the box is empty
void SPESavePlayerName(UIForm* inForm)
{
    UWideString theName(static_cast<UIEditBox*>(inForm->GetControl(kSinglePlayerPlayerNameEditBox))->GetText(0));
    if (theName.GetLength() > 0)
        gPlayerName = theName;
}

// FUNCTION: EXE 0x5e8b10
// Saves the typed player name and opens the scenario load screen (forgetting the saved main game form)
long EEUIESPECivCampaign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gSavedMainGameForm = NULL;
    SPESavePlayerName(inForm);
    gUIManager->Push(inForm->GetManager()->GetFormByID(kFormScenarioLoad));
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8b44
// Opens the single-player saved-game load screen
long EEUIESPELoadGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUIManager->Push(inForm->GetManager()->GetFormByID(kFormSinglePlayerLoadGame));
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8b68
// Saves the typed player name and returns to the previous screen
long EEUIESPEMainMenu::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    SPESavePlayerName(inForm);
    inForm->GetManager()->Pop();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8b86
// Starts a random-map game: you against one computer in the lobby, slots 2-15 closed, on a new local server
// (an empty name uses the default Human name; an invalid name shows a message and does nothing)
long EEUIESPERandomMap::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    EEGameOptionsPlayer theComputer;
    EEGameOptionsPlayer theHuman;
    EEGameOptionsPlayer theClosed;
    UIManager* theManager = inForm->GetManager();
    gWorld.GetLobbyOptions().mLock->WriteLock();
    gGameOptions = gGamePreferences.GetLastGameOptions();
    theName = static_cast<UIEditBox*>(inForm->GetControl(kSinglePlayerPlayerNameEditBox))->GetText(0);
    if (theName.GetLength() == 0)
        gResource->GetString(IDS_SE_HUMAN, theName);
    else
        gPlayerName = theName;
    if (EEIsValidName(theName)) {
        // the slot table is Rust's (ee_screens single_player.rs random_map_slots): the human, the computer, the
        // closed slots (a new slot's, as the original's)
        EeScreensSinglePlayerSlot theSlots[16];
        ee_screens_single_player_slots(theSlots);
        UWideString theComputerName;
        gResource->GetString(IDS_COMPUTER, theComputerName);
        gWorld.GetLobbyOptions().mIsMultiplayer = false;
        for (unsigned long i = 0; i < 16; i++) {
            const EeScreensSinglePlayerSlot& theSlot = theSlots[i];
            EEGameOptionsPlayer& thePlayer = theSlot.source == EE_SCREENS_SP_HUMAN ? theHuman
                                           : theSlot.source == EE_SCREENS_SP_COMPUTER ? theComputer : theClosed;
            if (theSlot.source != EE_SCREENS_SP_CLOSED) {
                thePlayer.mType = theSlot.kind;
                thePlayer.mColor = theSlot.color;
                thePlayer.mIsLocal = theSlot.local;
                thePlayer.mIsReady = theSlot.ready;
                thePlayer.mName = theSlot.source == EE_SCREENS_SP_HUMAN ? theName : theComputerName;
            }
            thePlayer.mSlot = theSlot.slot;
            gWorld.GetLobbyOptions().mPlayers[i] = thePlayer;
        }
        gWorld.GetLobbyOptions().mLock->WriteUnlock();
        UIEvent* theEvent = new EEUIELEActivateSinglePlayer;
        theManager->GetFormByID(kFormLobby)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        if (gCommunications)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        gCommunications->ProcessLocalUserInput(
            new NEMClientConnectionRequest(theHuman.mName, gGameVersion, gBinaryChecksum, false), kSlotNone);
    } else {
        gResource->GetString(IDS_PLAYER_NAME_IS_INVALID, theName);
        gUIManager->DisplayMessage(theName, true, NULL, 0, true);
        gWorld.GetLobbyOptions().mLock->WriteUnlock();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8e52
// Destroys the lobby's single-player activate event and frees it when asked (scalar deleting destructor)
// (EEUIELEActivateSinglePlayer's ??_G, after its creator RandomMap)
