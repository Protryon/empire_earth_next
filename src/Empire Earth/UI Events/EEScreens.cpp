// C:\EEX\Empire Earth\UI Events\EEScreens.cpp (NEW, no original address: units.EXTRA_UNITS). The host of the game
// screens that are Rust's whole (rust/crates/ee_screens menus.rs: the main menu, the editors' sub-menu, the single
// player screen; docs/RUST_PLAN.md "R6 game screens", step 2): EEScreenEvent's ProcessEvent, the C++ events the
// screens still hang on other forms, and the toolkit's and the game's calls the Rust dispatchers make
// (ee_screenscpp_*). It replaces EEUIEMainScreenEvents.cpp, EEUIEMultiplayerScreenEvents.cpp,
// EEUIEEmpireEarthEditorsEvents.cpp and EEUIESinglePlayerEvents.cpp.
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
#include "ee/EEDatabaseTables.h"
#include "ee/EECivilization.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGameState.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEMainGameEvents.h"
#include "ee/EEUIEGameSettingsEvents.h"
#include "ee/EEUIELobbyEvents.h"
#include "ee/EEUIEPlayCampaignEvents.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"

UString gMultiplayerString("");             // 0x562fd8 (EEUserInterface.cpp keeps the WON password in it)
static EECivilization sCivilization;        // 0x962f08: the civilization the editors' sub-menu opens the builder on

long EEScreenEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return ee_screens_event(this->mEvent, inForm, inEntity);
}

UIEvent* EECreateScreenEvent(unsigned long inEvent)
{
    switch (inEvent) {
    case 0:
        return NULL;
    case EE_SCREENS_CPP_SETTINGS_OPEN:
        return new EEUIEGSEOutOfGameOpen;
    case EE_SCREENS_CPP_CAMPAIGN_OPEN:
        return new EEUIEPCEFormOpen;
    case EE_SCREENS_CPP_LOBBY_SINGLE:
        return new EEUIELEActivateSinglePlayer;
    case EE_SCREENS_CPP_DUMP_MEDIA:
    case EE_SCREENS_CPP_DUMP_MEDIA_ALL:
        return new EEUIEMGDumpLoadedMedia(inEvent == EE_SCREENS_CPP_DUMP_MEDIA_ALL);
    }
    return new EEScreenEvent(inEvent);
}

static UWideString Text(const uint8_t* inText, size_t inLength)
{
    return UWideString(std::string((const char*)inText, inLength).c_str());
}

// the toolkit's calls (EE_SCREENS_OP_*) and the game's (EE_SCREENS_GAME_*) on the event's form
extern "C" void ee_screenscpp_call(void* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC, uint32_t inD)
{
    UIForm* theForm = static_cast<UIForm*>(inForm);
    EEUIManager* theManager = static_cast<EEUIManager*>(theForm->GetManager());
    switch (inOp) {
    case EE_SCREENS_OP_PUSH:
        theManager->Push(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_POP:
        theManager->Pop();
        break;
    case EE_SCREENS_OP_SWITCH:
        gUserInterface->SwitchToForm(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_OPEN_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inB);
        theManager->GetFormByID(inA)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_SHOW:
        theForm->GetControl(inA)->SetDisplayMode(inB);
        break;
    case EE_SCREENS_OP_HOT_KEY:
        theForm->AddHotKey(inA, inB, inC, kKeyNone, NULL, NULL, EECreateScreenEvent(inD), NULL, 0, false);
        break;
    case EE_SCREENS_GAME_QUIT:
        gUserInterface->PostQuit();
        break;
    case EE_SCREENS_GAME_EXTERNAL_LOBBY:
        EEStartExternalLobby();
        break;
    case EE_SCREENS_GAME_EDITOR_MODE_OFF:
        theManager->mInScenarioEditor = false;
        break;
    case EE_SCREENS_GAME_SINGLE:
    case EE_SCREENS_GAME_MULTI:
        gGameState.mGameOptions.mIsMultiplayer = inOp == EE_SCREENS_GAME_MULTI;
        break;
    case EE_SCREENS_GAME_DELETE_EDITOR:
        theManager->DeleteScenarioEditor();
        break;
    case EE_SCREENS_GAME_DELETE_WON:
        if (gWonNet != NULL) {
            EEWonNet* theWonNet = gWonNet;
            gWonNet = NULL;
            delete theWonNet;
        }
        break;
    case EE_SCREENS_GAME_CREATE_EDITOR:
        theManager->CreateScenarioEditor();
        break;
    case EE_SCREENS_GAME_START_EDITOR:
        theManager->StartScenarioEditor();
        break;
    case EE_SCREENS_GAME_CIV_BUILDER:
        sCivilization.mPurchasedUpgrades.clear();
        sCivilization.mAvailablePoints = sCivilization.mMaxPoints;
        sCivilization.SetPointsAndMax(gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints));
        gUserInterface->OpenCivilizationBuilder(&sCivilization, false);
        break;
    case EE_SCREENS_GAME_FORGET_MAIN_FORM:
        gSavedMainGameForm = NULL;
        break;
    case EE_SCREENS_GAME_LOBBY_BEGIN:
        gWorld.GetLobbyOptions().mLock->WriteLock();
        gGameOptions = gGamePreferences.GetLastGameOptions();
        break;
    case EE_SCREENS_GAME_LOBBY_END:
        gWorld.GetLobbyOptions().mLock->WriteUnlock();
        break;
    }
}

// a text of the game (EE_SCREENS_TEXT_*) into outText (up to inCapacity bytes); its length in bytes
extern "C" size_t ee_screenscpp_text(void* inForm, uint32_t inWhich, uint32_t inArg, uint8_t* outText, size_t inCapacity)
{
    UWideString theText;
    switch (inWhich) {
    case EE_SCREENS_TEXT_VERSION:
        sprintf(theText, "%s", (const char*)gGameVersion);
        break;
    case EE_SCREENS_TEXT_PLAYER:
        theText = gPlayerName;
        break;
    case EE_SCREENS_TEXT_EDIT:
        theText = static_cast<UIEditBox*>(static_cast<UIForm*>(inForm)->GetControl(inArg))->GetText(0);
        break;
    case EE_SCREENS_TEXT_STRING:
        gResource->GetString(inArg, theText);
        break;
    }
    size_t theLength = theText.GetTCHARLength();
    memcpy(outText, (const char*)theText, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

// a text into the game (EE_SCREENS_TEXT_*)
extern "C" void ee_screenscpp_put_text(void* inForm, uint32_t inWhich, uint32_t inArg, const uint8_t* inText,
                                       size_t inLength)
{
    UWideString theText = Text(inText, inLength);
    UIForm* theForm = static_cast<UIForm*>(inForm);
    switch (inWhich) {
    case EE_SCREENS_TEXT_PLAYER:
        gPlayerName = theText;
        break;
    case EE_SCREENS_TEXT_EDIT:
        static_cast<UIEditBox*>(theForm->GetControl(inArg))->SetText(theText);
        break;
    case EE_SCREENS_TEXT_LABEL:
        static_cast<UITextLabel*>(theForm->GetControl(inArg))->SetText(theText);
        break;
    case EE_SCREENS_TEXT_MESSAGE:
        gUIManager->DisplayMessage(theText, true, NULL, 0, true);
        break;
    case EE_SCREENS_TEXT_SERVER:
        // the random map's local server (the old communications deleted) and the human's connection request
        if (gCommunications)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        gCommunications->ProcessLocalUserInput(new NEMClientConnectionRequest(theText, gGameVersion, gBinaryChecksum, false),
                                               kSlotNone);
        break;
    }
}

// the random map's lobby options inside the write lock (EE_SCREENS_GAME_LOBBY_BEGIN): single player, the slots from
// the table (the human's and the computer's with their names, the others a new slot's with its slot number), then
// the unlock
extern "C" void ee_screenscpp_lobby_fill(const EeScreensSinglePlayerSlot* inSlots, const uint8_t* inHuman,
                                         size_t inHumanLength, const uint8_t* inComputer, size_t inComputerLength)
{
    EEGameOptionsPlayer theComputer;
    EEGameOptionsPlayer theHuman;
    EEGameOptionsPlayer theClosed;
    gWorld.GetLobbyOptions().mIsMultiplayer = false;
    for (unsigned long i = 0; i < 16; i++) {
        const EeScreensSinglePlayerSlot& theSlot = inSlots[i];
        EEGameOptionsPlayer& thePlayer = theSlot.source == EE_SCREENS_SP_HUMAN ? theHuman
                                       : theSlot.source == EE_SCREENS_SP_COMPUTER ? theComputer : theClosed;
        if (theSlot.source != EE_SCREENS_SP_CLOSED) {
            thePlayer.mType = theSlot.kind;
            thePlayer.mColor = theSlot.color;
            thePlayer.mIsLocal = theSlot.local;
            thePlayer.mIsReady = theSlot.ready;
            thePlayer.mName = theSlot.source == EE_SCREENS_SP_HUMAN ? Text(inHuman, inHumanLength)
                                                                    : Text(inComputer, inComputerLength);
        }
        thePlayer.mSlot = theSlot.slot;
        gWorld.GetLobbyOptions().mPlayers[i] = thePlayer;
    }
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
}
