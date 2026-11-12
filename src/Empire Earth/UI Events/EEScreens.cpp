// C:\EEX\Empire Earth\UI Events\EEScreens.cpp (NEW, no original address: units.EXTRA_UNITS). The host of the game
// screens that are Rust's whole (rust/crates/ee_screens menus.rs: the main menu, the editors' sub-menu, the single
// player and play campaign screens; settings_form.rs: the game settings; docs/RUST_PLAN.md "R6 game screens", step
// 2): EEScreenEvent's ProcessEvent, the C++ events the screens still hang on other forms, and the toolkit's and the
// game's calls the Rust dispatchers make (ee_screenscpp_*). It replaces EEUIEMainScreenEvents.cpp,
// EEUIEMultiplayerScreenEvents.cpp, EEUIEEmpireEarthEditorsEvents.cpp, EEUIESinglePlayerEvents.cpp,
// EEUIEPlayCampaignEvents.cpp and EEUIEGameSettingsEvents.cpp.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
#include <set>

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
#include "lle/UISlider.h"
#include "lle/UIComboBox.h"
#include "lle/UIESliderChange.h"
#include "lle/SEMixer.h"
#include "lle/SEConductor.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousServer.h"
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/FSRestorable.h"
#include "lle/UILayoutUtility.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"

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
#include "ee/EEUIELobbyEvents.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEMouse.h"
#include "ee/EEGameSound.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/RTSGraphics.h"
#include "ee/EEDisplayDevices.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEASaveGame.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECampaignProgress.h"
#include "ee/EEUIBMapBackground.h"
#include "ee/EEStringIDs.h"
#include "platform/PlatWindow.h"
#include "rust/ee_screens.h"

void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);   // lle/SEMixer.h
unsigned long EEMenuWidth();   // Empire Earth.cpp
unsigned long EEMenuHeight();

UString gMultiplayerString("");             // 0x562fd8 (EEUserInterface.cpp keeps the WON password in it)
EEUIFMainGame* gSavedMainGameForm;          // 0x563144 (.bss among the UI events' statics; ee/EmpireEarth.h)
static EECivilization sCivilization;        // 0x962f08: the civilization the editors' sub-menu opens the builder on

long EEScreenEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return ee_screens_event(this->mEvent, inForm, inEntity);
}

static UWideString sCampaignError;   // the last campaign load's error (EE_SCREENS_TEXT_CAMPAIGN_ERROR)

// the custom campaign form's no-campaigns message, closed
static void EEScreensNoCustomCampaigns(UIForm* inForm)
{
    ee_screens_event(EE_SCREENS_CC_NONE_FOUND, inForm, NULL);
}

// the settings form's Restore Default question, answered yes
static void EEScreensRestoreDefaultYes(UIForm* inForm)
{
    ee_screens_event(EE_SCREENS_GS_RESTORE_YES, inForm, NULL);
}

// ---- the load and save screens' (save_load.rs): the questions' callbacks get the list or the pending save object

// the pending save (an EEMLoadOrSaveGame or an EEASaveGame, as the "form" DisplayQuestion hands its callbacks) between
// its creation and the save event's end
static UIForm* sSaveObject;

// the delete question, answered yes: the list's selected file deleted
static void EEScreensDeleteSelectedFile(UIForm* inList)
{
    reinterpret_cast<UIFileListBox*>(inList)->DeleteSelectedFile();
}

// a saved game's save message to every player, waiting up to 15 s; the save handed on
static void EEScreensSendSave(UIForm* inMessage)
{
    static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(reinterpret_cast<NEMessage*>(inMessage), 15000, 12);
    ee_screens_event(EE_SCREENS_SL_SAVED, NULL, NULL);
}

// a scenario's save action posted, our reference dropped; the save handed on
static void EEScreensPostSave(UIForm* inAction)
{
    gUserInterface->PostAction(reinterpret_cast<EEAction*>(inAction));
    reinterpret_cast<EEAction*>(inAction)->Dereference();
    ee_screens_event(EE_SCREENS_SL_SAVED, NULL, NULL);
}

// the overwrite question, answered no: the pending save dropped
static void EEScreensDropSave(UIForm* inObject)
{
    reinterpret_cast<UReferenceCounter*>(inObject)->Dereference();
}

// ---- the campaign scenario select and scenario intro forms' (scenario_screens.rs)

static UString sCampaignDefaultUser("default");   // 0x5d2ae0 (gCampaignDefaultUser): the campaign progress's user

// the intro's scenario file (GetScenario's, for Start's load or game)
static FSFileSpec& EEScreensScenario()
{
    static FSFileSpec sScenario;
    return sScenario;
}

// a campaign scenario's sound and map below the campaign's scenario prefix
struct EEScreensMedia {
    static void Prefix(EESMMediaStrings& ioStrings)
    {
        ioStrings.mSoundFile = gU52A280 + ioStrings.mSoundFile;
        ioStrings.mMapFile = gU52A280 + ioStrings.mMapFile;
    }
};

// the campaign's open-error message, closed
static void EEScreensCloseCampaign(UIForm* inForm)
{
    ee_screens_event(EE_SCREENS_CS_CLOSE_CALLBACK, inForm, NULL);
}

// a put's texts joined by 0 bytes
static std::vector<UString> Parts(const uint8_t* inText, size_t inLength)
{
    std::vector<UString> theParts;
    std::string theText((const char*)inText, inLength);
    size_t theStart = 0;
    for (;;) {
        size_t theEnd = theText.find('\0', theStart);
        theParts.push_back(UString(theText.substr(theStart, theEnd == std::string::npos ? std::string::npos
                                                                                     : theEnd - theStart).c_str()));
        if (theEnd == std::string::npos)
            break;
        theStart = theEnd + 1;
    }
    while (theParts.size() < 3)
        theParts.push_back(UString(""));
    return theParts;
}

UIEvent* EECreateScreenEvent(unsigned long inEvent)
{
    switch (inEvent) {
    case 0:
        return NULL;
    case EE_SCREENS_CPP_LOBBY_SINGLE:
        return new EEUIELEActivateSinglePlayer;
    case EE_SCREENS_CPP_DUMP_MEDIA:
    case EE_SCREENS_CPP_DUMP_MEDIA_ALL:
        return new EEUIEMGDumpLoadedMedia(inEvent == EE_SCREENS_CPP_DUMP_MEDIA_ALL);
    }
    return new EEScreenEvent(inEvent);
}

static float Float(uint32_t inBits)
{
    float theValue;
    memcpy(&theValue, &inBits, 4);
    return theValue;
}
static uint32_t Bits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

static UWideString Text(const uint8_t* inText, size_t inLength)
{
    return UWideString(std::string((const char*)inText, inLength).c_str());
}

// the toolkit's calls (EE_SCREENS_OP_*) and the game's (EE_SCREENS_GAME_*) on the event's form and entity
extern "C" uint32_t ee_screenscpp_call(void* inForm, void* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                       uint32_t inC, uint32_t inD)
{
    UIForm* theForm = static_cast<UIForm*>(inForm);
    // (a posted event runs with no form: the game's manager)
    EEUIManager* theManager = theForm ? static_cast<EEUIManager*>(theForm->GetManager()) : gUIManager;
    auto Ctl = [&]() { return theForm->GetControl(inA); };   // a control op's control
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
    case EE_SCREENS_OP_OPEN_EVENT:
    case EE_SCREENS_OP_CLOSE_EVENT:
    case EE_SCREENS_OP_TRIGGER: {
        UIEvent* theEvent = EECreateScreenEvent(inOp == EE_SCREENS_OP_TRIGGER ? inD : inB);
        if (inOp == EE_SCREENS_OP_OPEN_EVENT)
            theManager->GetFormByID(inA)->SetFormOpenEvent(theEvent);
        else if (inOp == EE_SCREENS_OP_CLOSE_EVENT)
            theManager->GetFormByID(inA)->SetFormCloseEvent(theEvent);
        else
            theManager->GetFormByID(inA)->GetControl(inB)->SetTriggerEvent(inC, theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_OPEN_FORM:
        gUserInterface->OpenForm(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_ENTITY_INDEX:
        return static_cast<UIControl*>(inEntity)->GetIndex();
    case EE_SCREENS_OP_SHOW:
        if (UIControl* c = Ctl())
            c->SetDisplayMode(inB);
        break;
    // the settings form's (settings_form.rs): a missing control answers 0 and takes nothing
    case EE_SCREENS_OP_GROUP: {
        UIForm* theTarget = inC ? theManager->GetFormByID(inC - 1) : theForm;
        if (inB)
            theTarget->GetEntityGroup(inA)->Enable();
        else
            theTarget->GetEntityGroup(inA)->Disable();
        break;
    }
    case EE_SCREENS_OP_BACKGROUND:
        (inC ? theManager->GetFormByID(inC - 1) : theForm)->GetBackgroundByID(inA)->SetVisibility(inB != 0);
        break;
    case EE_SCREENS_OP_CHECKBOX:
        return Ctl() && static_cast<UICheckBox*>(Ctl())->GetIsChecked();
    case EE_SCREENS_OP_SET_CHECKBOX:
        if (UIControl* c = Ctl())
            static_cast<UICheckBox*>(c)->SetChecked(inB != 0);
        break;
    case EE_SCREENS_OP_RADIO:
        return Ctl() && static_cast<UIRadioButton*>(Ctl())->GetIsChecked();
    case EE_SCREENS_OP_CHECK_RADIO:
        if (UIControl* c = Ctl())
            static_cast<UIRadioButton*>(c)->Check();
        break;
    case EE_SCREENS_OP_RANGE:
        return Ctl() ? static_cast<UISlider*>(Ctl())->GetCurrentRangeValue() : 0;
    case EE_SCREENS_OP_SET_RANGE:
        if (UIControl* c = Ctl())
            static_cast<UISlider*>(c)->SetCurrentRangeValue((int32_t)inB);
        break;
    case EE_SCREENS_OP_ENTITY_RANGE:
        return static_cast<UISlider*>(inEntity)->GetCurrentRangeValue();
    case EE_SCREENS_OP_COMBO_SELECT:
        if (UIControl* c = Ctl())
            static_cast<UIComboBox*>(c)->SetCurrentSelection(inB);
        break;
    case EE_SCREENS_OP_LIST_SELECT:
        if (UIControl* c = Ctl())
            static_cast<UILBString*>(c)->ChangeSelectionIndex(inB, false, true);
        break;
    case EE_SCREENS_OP_COMBO_CLEAR:
        if (UIControl* c = Ctl())
            static_cast<UIComboBox*>(c)->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_LIST_CLEAR:
        if (UIControl* c = Ctl())
            static_cast<UILBString*>(c)->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_RELEASE_MOUSE:
        if (UIControl* c = Ctl())
            c->ReleaseCapturedMouse();
        break;
    case EE_SCREENS_OP_LABEL_NUMBER:
        if (UIControl* c = Ctl())
            static_cast<UITextLabel*>(c)->SetText((long)(int32_t)inB);
        break;
    case EE_SCREENS_OP_ENTITY_ID:
        return inEntity ? static_cast<UIControl*>(inEntity)->GetId() : 0xffffffffu;
    case EE_SCREENS_OP_CONTROL_ID:
        return Ctl() ? Ctl()->GetId() : 0;
    case EE_SCREENS_OP_TRIGGER_HERE: {
        UIEvent* theEvent = EECreateScreenEvent(inC);
        Ctl()->SetTriggerEvent(inB, theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_SLIDER_LABEL: {
        UIESliderChange* theEvent = new UIESliderChange(static_cast<UITextLabel*>(theForm->GetControl(inB)), 1.0f, 0);
        Ctl()->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
        if (inC) {
            UWideString theString;
            gResource->GetString(inC, theString);
            theEvent->AddString(0.0f, theString);
        }
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_CLOSE_FORM:
        gUserInterface->CloseForm(NULL);
        break;
    case EE_SCREENS_OP_DISPLAY_CHANGED:
        gUserInterface->PostDisplayChanged(false);
        break;
    case EE_SCREENS_OP_RESIZE_MENU:
        gUserInterface->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false);
        break;
    case EE_SCREENS_OP_SRAND_TIME:
        srand(TMGetTime());
        break;
    case EE_SCREENS_OP_RAND:
        return rand();
    case EE_SCREENS_GAME_CONDUCTOR_VOLUME:
        return gConductor->GetConductor().SetVolume(Float(inA));
    case EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME:
        return Bits(gConductor->GetConductor().GetVolume());
    case EE_SCREENS_GAME_REVERB:
        gConductor->GetConductor().SetReverb(inA);
        break;
    case EE_SCREENS_GAME_CONDUCTOR_STATE:
        gConductor->GetConductor().SetConductorState(inA);
        break;
    case EE_SCREENS_GAME_GET_CONDUCTOR_STATE:
        return gConductor->GetConductor().GetConductorState();
    case EE_SCREENS_GAME_UI_MUSIC_VOLUME:
        gUIManager->SetMusicVolume(Float(inA));
        break;
    case EE_SCREENS_GAME_MASTER_VOLUME:
        return gSoundManager->SetMasterVolume(Float(inA));
    case EE_SCREENS_GAME_GET_MASTER_VOLUME:
        return Bits(gSoundManager->GetMasterVolume());
    case EE_SCREENS_GAME_GET_CHANNELS:
        return gSoundManager->GetMaxChannels();
    case EE_SCREENS_GAME_CHANNELS:
        gSoundManager->SetMaxChannels(inA);
        break;
    case EE_SCREENS_GAME_MUSIC_START:
        gMusicManager->Start();
        break;
    case EE_SCREENS_GAME_MUSIC_STOP:
        gMusicManager->Stop();
        break;
    case EE_SCREENS_GAME_MOUSE_SENSITIVITY:
        gMouse->SetSensitivity(Float(inA));
        break;
    case EE_SCREENS_GAME_LEFT_HANDED:
        UIManager::SetMouseLeftHanded(inA != 0);
        break;
    case EE_SCREENS_GAME_IS_SERVER:
        return gCommunications->IsServer() == true;
    case EE_SCREENS_GAME_PAUSE:
        gWorld.PauseGame();
        break;
    case EE_SCREENS_GAME_CURRENT_RASTERIZER:
        for (unsigned long i = 0; i < GERasterizer::GetRasterizerCount(); i++)
            if (GERasterizer::GetRasterizer(i) == gRasterizer)
                return i;
        return 0;
    case EE_SCREENS_GAME_CHANGE_RASTERIZER:
        gUserInterface->PostChangeRasterizer(inA, inB);
        break;
    case EE_SCREENS_GAME_APPLY_PORT:
        EEApplyPortSettings(gGamePreferences.GetSettings());
        break;
    case EE_SCREENS_GAME_VIEW: {
        // the game view rectangle the settings reset
        GERectangle theRectangle;
        theRectangle.SetBottom(1085.0f);
        theRectangle.SetLeft(0.0f);
        theRectangle.SetRight(1600.0f);
        theRectangle.SetTop(35.0f);
        EESetGameViewRectangle(theRectangle);
        break;
    }
    case EE_SCREENS_GAME_MIP_FILTER:
        gTextureManager->UpdateMIPFilterMode(inA);
        break;
    case EE_SCREENS_GAME_TEXTURE_RESOLUTION:
        EESetTextureResolution((int32_t)inA);
        break;
    case EE_SCREENS_GAME_AMBIENT:
        gAmbientSoundManager.SetEnabled(inA != 0);
        break;
    case EE_SCREENS_GAME_SAVE:
        return gGamePreferences.Save();
    case EE_SCREENS_GAME_NOT_KOREAN:
        return gGamePreferences.IsNotKorean();
    case EE_SCREENS_GAME_TEST_SOUND:
        if (EESoundObject* theSound = EESoundObject::Get(inA)) {
            theSound->LoadMedia();
            gSoundManager->Play(theSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        }
        break;
    case EE_SCREENS_GAME_WINDOW_MODE: {
        // the settings' window mode on the game window (the caller resizes the game)
        PlatWindow* theWindow = gGameWindow ? gGameWindow->GetOSRepresentation() : NULL;
        if (!theWindow)
            break;
        if (gGamePreferences.GetSettings().mWindowMode == 0) {
            PlatWindowSetFixedSize(theWindow, 0);
            PlatWindowSetFullscreen(theWindow, 1);
        } else {
            int32_t theWidth = 0, theHeight = 0;
            PlatWindowSetFullscreen(theWindow, 0);
            PlatWindowSetFixedSize(theWindow, 0);
            if (PlatWindowGetDisplayLogicalSize(NULL, &theWidth, &theHeight) && theWidth > 0)
                PlatWindowSetSize(theWindow, theWidth * 4 / 5, theHeight * 4 / 5);
            PlatWindowSetFixedSize(theWindow, 1);
        }
        break;
    }
    // the custom campaign form's (custom_campaign.rs): a list box of form inA
    case EE_SCREENS_OP_FORM_LIST_CLEAR:
        static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_FORM_LIST_COUNT:
        return static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->GetLineCount();
    case EE_SCREENS_OP_FORM_LIST_SELECT:
        static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->ChangeSelectionIndex(inC, true, false);
        break;
    case EE_SCREENS_OP_FORM_LIST_SELECTION: {
        unsigned long theSelection = 0;
        static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->GetCurrentSelection(theSelection);
        return theSelection;
    }
    case EE_SCREENS_GAME_RASTERIZER_COUNT:
        return (uint32_t)gDisplayRasterizers.size();
    case EE_SCREENS_GAME_SCREEN_COUNT:
        return inA < gDisplayRasterizers.size() ? (uint32_t)gDisplayRasterizers[inA].mScreens.size() : 0;
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
    case EE_SCREENS_GAME_SHOW_SINGLE_PLAYER:
        EEShowSinglePlayerScreen();
        break;
    case EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN:
        EECampaignFormManager::Get()->mAllScenariosOpen = inA != 0;
        break;
    // the load and save screens' (save_load.rs)
    case EE_SCREENS_OP_SWAP_FORM:
        theManager->SwapForm(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_CURRENT_FORM_IS:
        return theManager->GetCurrentForm() == theManager->GetFormByID(inA);
    case EE_SCREENS_OP_POST_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_FORM_LIST_SELECTED: {
        UIForm* theTarget = inA ? theManager->GetFormByID(inA - 1) : theForm;
        UILBString* theList = static_cast<UILBString*>(theTarget->GetControl(inB));
        unsigned long theSelection = 0;
        if (theList && theList->GetCurrentSelection(theSelection) == kErrorOK)
            return (uint32_t)theSelection;
        return 0xffffffffu;
    }
    case EE_SCREENS_GAME_FREE_CAMERA:
        return gFreeCamera;
    case EE_SCREENS_GAME_MULTIPLAYER_SESSION:
        return gCommunications->GetIsMultiplayerSession();
    case EE_SCREENS_GAME_SCREEN_ACTIVE:
        return gGameScreenActive;
    case EE_SCREENS_GAME_IN_EDITOR:
        return theManager->mInScenarioEditor;
    case EE_SCREENS_GAME_PAUSE_SERVER: {
        // (the save screen's original did not check the cast: a client crashed there)
        EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
        if (theServer && theServer->GetIsGamePaused() == false) {
            theServer->PauseGame();
            gUserInterface->PostPauseInformation(true);
        }
        break;
    }
    case EE_SCREENS_GAME_CTS_PROCESSED:
        return gCommunications->GetNumberOfCTsProcessedInPhysics();
    case EE_SCREENS_GAME_SAVE_SEND:
        EEScreensSendSave(sSaveObject);
        break;
    case EE_SCREENS_GAME_SAVE_POST:
        EEScreensPostSave(sSaveObject);
        break;
    case EE_SCREENS_GAME_SAVED_MAIN_FORM:
        return gSavedMainGameForm != NULL;
    case EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM:
        gUserInterface->OpenForm(gSavedMainGameForm);
        break;
    // the campaign scenario select and scenario intro forms' (scenario_screens.rs)
    case EE_SCREENS_OP_REMOVE_BACKGROUND:
        if (UIBackground* theBackground = theForm->GetBackgroundByID((long)(int32_t)inA))
            theForm->RemoveBackground(theBackground);
        break;
    case EE_SCREENS_OP_TAB_STOP:
        theForm->SetTabStop(inA);
        break;
    case EE_SCREENS_OP_LABEL_CLEAR:
        static_cast<UITextLabel*>(Ctl())->DeleteArray();
        break;
    case EE_SCREENS_OP_LABEL_LINE_COUNT:
        return static_cast<UITextLabel*>(Ctl())->GetLineCount();
    case EE_SCREENS_OP_LABEL_START_LINE:
        static_cast<UITextLabel*>(Ctl())->SetStartingDisplayLine(inB);
        break;
    case EE_SCREENS_OP_ADD_TRIGGER: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        theManager->AddEventToTrigger(theEvent, theForm, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_SLEEP:
        PlatSleep(inA);
        break;
    case EE_SCREENS_OP_OPEN_MAIN_GAME_FORM:
        gUserInterface->OpenForm(gUIManager->mMainGameForm);
        break;
    case EE_SCREENS_GAME_SI_FLAGS: {
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        return (theIntro->mIsInGame ? EE_SCREENS_SI_FLAG_IN_GAME : 0) |
               (theIntro->mIsLoadingFromGame ? EE_SCREENS_SI_FLAG_LOADING : 0) |
               (theIntro->mIsCampaign ? EE_SCREENS_SI_FLAG_CAMPAIGN : 0) |
               (theIntro->mScrollBriefingToEnd ? EE_SCREENS_SI_FLAG_SCROLL : 0);
    }
    case EE_SCREENS_GAME_SI_SET_FLAG: {
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        bool* const theFlags[4] = {&theIntro->mIsInGame, &theIntro->mIsLoadingFromGame, &theIntro->mIsCampaign,
                                   &theIntro->mScrollBriefingToEnd};
        for (unsigned long i = 0; i < 4; i++)
            if (inA == (1u << i))
                *theFlags[i] = inB != 0;
        break;
    }
    case EE_SCREENS_GAME_SI_BLINK_OFF:
        EEUISCIFormManager::Get()->SetBlinking(false);
        break;
    case EE_SCREENS_GAME_SOUND_STOP:
        gSoundManager->Stop(inA, false);
        break;
    case EE_SCREENS_GAME_SOUND_LATENCY:
        gSoundManager->SetLatency(inA);
        break;
    case EE_SCREENS_GAME_SI_OPEN_MAP:
        return EEUISCIFormManager::Get()->OpenMap();
    case EE_SCREENS_GAME_SI_OPEN_MOVIE:
        return EEUISCIFormManager::Get()->OpenMovie();
    case EE_SCREENS_GAME_SI_OPEN_SOUND:
        return EEUISCIFormManager::Get()->OpenSound();
    case EE_SCREENS_GAME_SI_PLAY_MOVIE: {
        FSFileSpec theMovie;
        UIEvent* theEvent = EECreateScreenEvent(inA);
        EEUISCIFormManager::Get()->GetMovie(theMovie);
        gUserInterface->PostPlayMovie(theMovie, theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_GAME_SI_PLAY_SOUND: {
        // the briefing sound once (below Sounds\ unless its file names the folder)
        FSFileSpec theSound;
        UString theFile;
        UString theLower;
        UString theDirectory;
        unsigned long theSample;
        unsigned long theID = inA;
        EEUISCIFormManager::Get()->GetSound(theSound);
        theFile = theSound.GetFile();
        theLower = theFile;
        theLower.ToLower();
        theDirectory = UWideString("Sounds\\");
        theDirectory.ToLower();
        if (theLower.Find(theDirectory, 0) == theLower.GetLength())
            theFile = UString("Sounds\\") + theFile;
        gSoundManager->SetLatency(500);
        EELoadSound(theFile, 0x31, theSample, theID, 0);
        gSoundManager->RegisterOneShotSound(theSample, theID);
        return (uint32_t)theID;
    }
    case EE_SCREENS_GAME_SI_MAP_BACKDROP: {
        // the scenario's map picture as the form's backdrop (-2), unless it already is
        FSFileSpec theMap;
        UString theFile;
        UString theLower;
        UString theDirectory;
        GERectangle theRectangle;
        GETextureDescription theDescription;
        theDescription.mPixelDepth = 16;
        theDescription.mNumMipLevelsToDrop = 0;
        theDescription.mSurfaceVolatility = 1;
        theDescription.mFrameFormat = 0;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mHasAlpha = true;
        theDescription.mCreateMIPMaps = true;
        UILayoutUtility theLayout;
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        UIBackground* theBackground = theForm->GetBackgroundByID(-2);
        theIntro->GetMap(theMap);
        theFile = theMap.GetFile();
        theLower = theFile;
        theLower.ToLower();
        theDirectory = UWideString("Scenarios\\");
        theDirectory.ToLower();
        if (theLower.Find(theDirectory, 0) == theLower.GetLength())
            theFile = UString("Scenarios\\") + theFile;
        if (theBackground == NULL || strcmp(static_cast<UITextureBackground*>(theBackground)->GetTextureFileName(), theFile) != 0) {
            if (theBackground)
                theForm->RemoveBackground(theBackground);
            if (theIntro->OpenMap()) {
                theForm->LockDrawBackground();
                UIBackground* theBackdrop = new EEUIBMapBackground(-2, theFile, theDescription, theRectangle, theLayout,
                                                                   theForm, 2, 0, 0, true);
                theBackdrop->Open();
                theForm->UnlockDrawBackground();
            }
        }
        break;
    }
    case EE_SCREENS_GAME_SI_GET_SCENARIO:
        return EEUISCIFormManager::Get()->GetScenario(EEScreensScenario());
    case EE_SCREENS_GAME_SI_POST_LOAD:
        gUserInterface->PostLoadSavedGame(EEScreensScenario());
        break;
    case EE_SCREENS_GAME_SI_START_GAME: {
        // a single player game of the scenario (inA: the campaign's), a new local server, start loading
        gGameOptions.mLock->WriteLock();
        gGameOptions.SetDefaults();
        if (inA) {
            EECampaignFormManager* theCampaign = EECampaignFormManager::Get();
            gGameOptions.mLoadType = 4;
            gGameOptions.mDifficulty = theCampaign->mDifficultyLevel;
            gGameOptions.mCampaignName = theCampaign->mCampaignName;
            gGameOptions.mCampaignChecksum = theCampaign->mChecksum;
            gGameOptions.mScenarioName = theCampaign->mScenarioName;
            gGameOptions.mMapName = theCampaign->mFirstScenario;
        } else {
            gGameOptions.mLoadType = 1;
            gLoadScenarioName = EEScreensScenario().GetFile().GetIndexedSubstring(0, EEScreensScenario().GetFile().GetLength() - 5);
        }
        gGameOptions.SetMapType((const char*)EEScreensScenario().GetFile());
        gGameOptions.mIsMultiplayer = false;
        gGameOptions.mLock->WriteUnlock();
        if (gCommunications)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        static_cast<EEServer*>(gCommunications)->StopPing();
        static_cast<EEServer*>(gCommunications)->mLoadState = true;
        NEMessage* theMessage = new EEMStartLoading(1, TMGetTime());
        gLocalPlayerOut = false;
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        break;
    }
    case EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM:
        EECampaignFormManager::Get()->UpdateChecksum();
        break;
    case EE_SCREENS_GAME_CAMPAIGN_CHECKSUM:
        return EECampaignFormManager::Get()->mChecksum;
    case EE_SCREENS_GAME_CS_MAP_BACKDROP: {
        // the campaign's background image (a file of Scenarios) as the form's backdrop (-2)
        GERectangle theRectangle;
        GETextureDescription theDescription;
        theDescription.mPixelDepth = 16;
        theDescription.mNumMipLevelsToDrop = 0;
        theDescription.mSurfaceVolatility = 1;
        theDescription.mFrameFormat = 0;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mHasAlpha = true;
        theDescription.mCreateMIPMaps = true;
        UILayoutUtility theLayout;
        FSFileSpec* theMapSpec = NULL;
        UWideString theMapName = EECampaignFormManager::Get()->mCampaign.GetBackgroundImage();
        UString theMapFile = UString("Scenarios\\" + gU52A280 + theMapName);
        if (gDataPath->FindFile(theMapFile, theMapSpec) == kErrorOK) {
            theForm->LockDrawBackground();
            UIBackground* theMap = new EEUIBMapBackground(-2, theMapFile, theDescription, theRectangle, theLayout, theForm,
                                                          2, 0, 0, true);
            theMap->Open();
            theForm->UnlockDrawBackground();
            if (theMapSpec)
                delete theMapSpec;
        }
        break;
    }
    case EE_SCREENS_GAME_DIFFICULTY:
        return (uint32_t)gDifficultyLevel;
    case EE_SCREENS_GAME_CS_SET_DIFFICULTY:
        EECampaignFormManager::Get()->mDifficultyLevel = (long)inA;
        gDifficultyLevel = (long)inA;
        gDifficultyLevel2 = (long)inA;
        gGameOptions.mLock->WriteLock();
        gGameOptions.mDifficulty = inA;
        gGameOptions.mLock->WriteUnlock();
        break;
    case EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET:
        return EECampaignFormManager::Get()->mAllScenariosOpen;
    }
    return 0;
}

// the manager's form of an id (scenario_screens.rs: a call on a named form)
extern "C" void* ee_screenscpp_form(uint32_t inID)
{
    return gUIManager->GetFormByID(inID);
}

// a text of the game (EE_SCREENS_TEXT_*) into outText (up to inCapacity bytes); its length in bytes
extern "C" size_t ee_screenscpp_text(void* inForm, uint32_t inWhich, uint32_t inArg, uint8_t* outText, size_t inCapacity)
{
    UWideString theText;
    UIForm* theForm = static_cast<UIForm*>(inForm);
    std::string theList;   // the lists' texts, each ended by a 0 byte
    switch (inWhich) {
    case EE_SCREENS_TEXT_MUSIC_FILE:
        theText = theForm->GetMusicFileName();
        break;
    case EE_SCREENS_TEXT_RASTERIZER_SETTING:
        theText = (inArg ? EEGamePreferences::sDefaultSettings : gGamePreferences.GetSettings()).mRasterizerName;
        break;
    case EE_SCREENS_TEXT_AUDIO_DEVICE:
        theText = (inArg ? EEGamePreferences::sDefaultSettings : gGamePreferences.GetSettings()).mAudioDevice;
        break;
    case EE_SCREENS_TEXT_DISPLAY_RASTERIZER:
        if (inArg < gDisplayRasterizers.size())
            theText = gDisplayRasterizers[inArg].mName;
        break;
    case EE_SCREENS_TEXT_DISPLAY_SCREEN:
        if ((inArg >> 16) < gDisplayRasterizers.size() &&
            (inArg & 0xffff) < gDisplayRasterizers[inArg >> 16].mScreens.size())
            theText = gDisplayRasterizers[inArg >> 16].mScreens[inArg & 0xffff].mName;
        break;
    case EE_SCREENS_TEXT_RASTERIZER_NAME:
        if (inArg < GERasterizer::GetRasterizerCount())
            theText = GERasterizer::GetRasterizer(inArg)->GetName();
        break;
    case EE_SCREENS_TEXT_LIST_DATA: {
        UILBString* theBox = static_cast<UILBString*>(theForm->GetControl(inArg >> 16));
        if ((inArg & 0xffff) < theBox->GetLineCount())
            theText = theBox->GetData(inArg & 0xffff);
        break;
    }
    case EE_SCREENS_TEXT_COMBO_DATA: {
        const UILBString* theBox = static_cast<UIComboBox*>(theForm->GetControl(inArg >> 16))->GetListBox();
        if ((inArg & 0xffff) < theBox->GetLineCount())
            theText = theBox->GetData(inArg & 0xffff);
        break;
    }
    case EE_SCREENS_TEXT_FORM_LIST_DATA: {
        UIListBox<UWideString>* theBox =
            static_cast<UIListBox<UWideString>*>(gUIManager->GetFormByID(inArg >> 24)->GetControl((inArg >> 16) & 0xff));
        if ((inArg & 0xffff) < theBox->GetLineCount())
            theText = theBox->GetData(inArg & 0xffff);
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_FILES: {
        // the files of data\Campaigns with the campaign extension (a set: in its order)
        std::set<UString> theFiles;
        FSFileSpec theDirectory;
        FSPath* thePath = new FSPath;
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
        thePath->AddDirectory(theDirectory, true);
        UString thePattern = UString("*") + gCampaignExtension;
        thePath->ListFiles(thePattern, theFiles);
        thePath->Dereference();
        for (std::set<UString>::iterator it = theFiles.begin(); it != theFiles.end(); it++)
            theList.append((const char*)*it).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_EXTENSION:
        theText = gCampaignExtension;
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_ERROR:
        theText = sCampaignError;
        break;
    case EE_SCREENS_TEXT_ADAPTERS: {
        NEProtocol* theProtocol = NULL;
        std::vector<UWideString> theAdapters;
        NEProtocol::GetProtocol(0, theProtocol);
        if (theProtocol)
            theProtocol->EnumerateNetworkAdapters(theAdapters);
        for (size_t i = 0; i < theAdapters.size(); i++)
            theList.append((const char*)theAdapters[i], theAdapters[i].GetTCHARLength()).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_OUTPUT_DEVICES: {
        char theNames[16][128];
        unsigned long theCount = SPIMGetOutputDevices(theNames, 16);
        for (unsigned long i = 0; i < theCount && i < 16; i++)
            theList.append(theNames[i]).push_back('\0');
        break;
    }
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
    case EE_SCREENS_TEXT_STRING_CHECKED:
        if (gResource->GetString(inArg, theText) != 0)
            return (size_t)-1;
        break;
    case EE_SCREENS_TEXT_CAMPAIGN: {
        const UWideString* const kCampaigns[4] = {&gCampaignAOCRoman, &gCampaignAOCPacific, &gCampaignAOCAsian,
                                                  &gCampaignEELearning};
        theText = *kCampaigns[inArg & 3];
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_LOADED:
        theText = EECampaignFormManager::Get()->mCampaign.GetDescription();
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_CIV:
        theText = EECampaignFormManager::Get()->mCampaign.GetRequiredCiv();
        break;
    case EE_SCREENS_TEXT_SAVED_EXTENSION:
        theText = gSavedGameExtension;
        break;
    case EE_SCREENS_TEXT_SCENARIO_EXTENSION:
        theText = gScenarioExtension;
        break;
    case EE_SCREENS_TEXT_SI_BRIEFING:
        theText = EEUISCIFormManager::Get()->GetBriefing();
        break;
    case EE_SCREENS_TEXT_SI_HINTS:
        theText = EEUISCIFormManager::Get()->GetHints();
        break;
    case EE_SCREENS_TEXT_SI_HISTORY:
        theText = EEUISCIFormManager::Get()->GetHistory();
        break;
    case EE_SCREENS_TEXT_CS_CAMPAIGN_NAME:
        theText = EECampaignFormManager::Get()->mCampaignName;
        break;
    case EE_SCREENS_TEXT_CS_DESCRIPTION:
        theText = EECampaignFormManager::Get()->mDescription;
        break;
    case EE_SCREENS_TEXT_CS_SCENARIOS: {
        std::vector<UWideString> theScenarios = EECampaignFormManager::Get()->mCampaign.GetScenarios();
        for (size_t i = 0; i < theScenarios.size(); i++)
            theList.append((const char*)theScenarios[i], theScenarios[i].GetTCHARLength()).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_CS_COMPLETED: {
        // the campaign progress of the default user: its completed scenarios (none when it does not load)
        EECampaignProgress theProgress;
        std::vector<UWideString> theCompleted;
        EECampaignFormManager* theManager = EECampaignFormManager::Get();
        if (theProgress.Load(UWideString(sCampaignDefaultUser), theManager->mCampaignName, theManager->GetChecksum(),
                             false) == kErrorOK)
            theProgress.GetCompletedScenarios(theCompleted);
        for (size_t i = 0; i < theCompleted.size(); i++)
            theList.append((const char*)theCompleted[i], theCompleted[i].GetTCHARLength()).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_CS_PROGRESS: {
        UWideString theString;
        gResource->GetString(IDS_CAMPAIGN_IN_PROGRESS_OF_COMPLETED, theString);
        sprintf(theText, theString, (size_t)(inArg >> 16), (size_t)(inArg & 0xffff));   // (the sizes, as the original)
        break;
    }
    case EE_SCREENS_TEXT_LOAD_SAVED_NAME:
        theText = gLoadSavedGameName;
        break;
    case EE_SCREENS_TEXT_LOAD_SCENARIO_NAME:
        theText = gLoadScenarioName;
        break;
    }
    if (!theList.empty()) {
        memcpy(outText, theList.data(), theList.size() < inCapacity ? theList.size() : inCapacity);
        return theList.size();
    }
    size_t theLength = theText.GetTCHARLength();
    memcpy(outText, (const char*)theText, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

// a text into the game (EE_SCREENS_TEXT_*); 1 for a campaign that loads
extern "C" uint32_t ee_screenscpp_put_text(void* inForm, uint32_t inWhich, uint32_t inArg, const uint8_t* inText,
                                           size_t inLength)
{
    UWideString theText = Text(inText, inLength);
    UIForm* theForm = static_cast<UIForm*>(inForm);
    switch (inWhich) {
    case EE_SCREENS_TEXT_PLAYER:
        gPlayerName = theText;
        break;
    case EE_SCREENS_TEXT_MUSIC_FILE:
        theForm->SetMusicFileName(theText);
        break;
    case EE_SCREENS_TEXT_RASTERIZER_SETTING:
        gGamePreferences.GetSettings().mRasterizerName = std::string((const char*)inText, inLength).c_str();
        break;
    case EE_SCREENS_TEXT_AUDIO_DEVICE: {
        EEGameSettings& theSettings = gGamePreferences.GetSettings();
        snprintf(theSettings.mAudioDevice, sizeof(theSettings.mAudioDevice), "%s",
                 std::string((const char*)inText, inLength).c_str());
        break;
    }
    case EE_SCREENS_TEXT_COMBO_APPEND:
        if (UIComboBox* theBox = static_cast<UIComboBox*>(theForm->GetControl(inArg)))
            theBox->AppendLine(theText);
        break;
    case EE_SCREENS_TEXT_COMBO_INSERT:
        if (UIComboBox* theBox = static_cast<UIComboBox*>(theForm->GetControl(inArg >> 16)))
            theBox->InsertLine(theText, inArg & 0xffff);
        break;
    case EE_SCREENS_TEXT_LIST_APPEND:
        if (UILBString* theBox = static_cast<UILBString*>(theForm->GetControl(inArg)))
            theBox->AppendLine(theText);
        break;
    case EE_SCREENS_TEXT_FORM_LIST_APPEND:
        static_cast<UIListBox<UWideString>*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff))->AppendLine(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN:
        return EECampaignFormManager::GetBuiltInCampaign(theText);   // (its number: the checksum it must have)
    case EE_SCREENS_TEXT_MESSAGE_NONE_FOUND:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensNoCustomCampaigns, true);
        break;
    case EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensCloseCampaign, true);
        break;
    case EE_SCREENS_TEXT_CS_ARCHIVE: {
        // the campaign's file on the data path; a valid one's archive added to it
        FSFileSpec* theArchiveFile = NULL;
        FSArchive theArchive;
        long theResult = gDataPath->FindFile(Parts(inText, inLength)[0], theArchiveFile);
        bool theAdded = theResult == kErrorOK && inArg != 0;
        if (theAdded) {
            theArchive.SetArchive(*theArchiveFile);
            gDataPath->AddArchive(theArchive, true);
            EESetDataPaths();
        }
        if (theArchiveFile)
            delete theArchiveFile;
        return theAdded;
    }
    case EE_SCREENS_TEXT_CS_SCENARIO_HEADER:
    case EE_SCREENS_TEXT_CS_START: {
        // a campaign scenario's header: for Start, its media strings (the campaign's prefix on the sound and map) and
        // file into the intro form's manager (2; 1: the header does not load, 0: no file)
        EEPSaveGameHeader theHeader;
        FSFileSpec* theFile = NULL;
        if (gDataPath->FindFile(UString("Scenarios\\" + gU52A280 + theText + gScenarioExtension), theFile) != kErrorOK) {
            if (theFile)
                delete theFile;
            return 0;
        }
        if (theHeader.LoadFromFile(*theFile) != kErrorOK) {
            delete theFile;
            return inWhich == EE_SCREENS_TEXT_CS_START ? 1 : 0;
        }
        if (inWhich == EE_SCREENS_TEXT_CS_SCENARIO_HEADER) {
            delete theFile;
            return 1;
        }
        EESMMediaStrings theMediaStrings;
        theMediaStrings = theHeader.mMediaStrings;
        EEUISCIFormManager* theInfo = EEUISCIFormManager::Get();
        EEScreensMedia::Prefix(theMediaStrings);
        EECampaignFormManager::Get()->UpdateChecksum();
        theInfo->mMediaStrings = theMediaStrings;
        theInfo->SetFile(theFile);
        theInfo->mIsCampaign = true;
        return 2;
    }
    case EE_SCREENS_TEXT_CS_SCENARIO_NAME:
        EECampaignFormManager::Get()->mScenarioName = theText;
        break;
    case EE_SCREENS_TEXT_QUESTION:
        gUIManager->DisplayQuestion(theText, true, theForm, EEScreensRestoreDefaultYes, NULL);
        break;
    case EE_SCREENS_TEXT_EDIT:
        static_cast<UIEditBox*>(theForm->GetControl(inArg))->SetText(theText);
        break;
    case EE_SCREENS_TEXT_LABEL:
        static_cast<UITextLabel*>(theForm->GetControl(inArg))->SetText(theText);
        break;
    case EE_SCREENS_TEXT_MESSAGE:
        gUIManager->DisplayMessage(theText, true, NULL, 0, inArg != 0);
        break;
    case EE_SCREENS_TEXT_LABEL_LINES: {
        UITextLabel* theLabel = static_cast<UITextLabel*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff));
        theLabel->DeleteArray();
        theLabel->AppendText(theText);
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_NAME:
        EECampaignFormManager::Get()->SetCampaignName(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION:
        EECampaignFormManager::Get()->SetDescription(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_FIRST:
        EECampaignFormManager::Get()->SetFirstScenario(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_LOAD: {
        UWideString theFile;
        UWideString theError;
        theFile = "Campaigns\\" + theText + gCampaignExtension;
        bool theLoaded = EECampaignFormManager::Get()->mCampaign.Load(theFile, theError, false);
        sCampaignError = theError;
        return theLoaded ? 1 : 0;
    }
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
    // the load and save screens' (save_load.rs)
    case EE_SCREENS_TEXT_LOAD_SAVED_NAME:
        gGamePreferences.mSettings.SetLoadSavedGameName(Parts(inText, inLength)[0]);
        break;
    case EE_SCREENS_TEXT_LOAD_SCENARIO_NAME:
        gGamePreferences.mSettings.SetLoadScenarioName(Parts(inText, inLength)[0]);
        break;
    case EE_SCREENS_TEXT_FILE_LIST: {
        // a file list from a directory below the startup one with an extension, the name selected when listed
        std::vector<UString> theParts = Parts(inText, inLength);
        UIFileListBox* theList = static_cast<UIFileListBox*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff));
        if (theList == NULL)
            break;
        FSFileSpec theDirectory;
        FSPath* thePath = new FSPath;
        UWideString theName(theParts[2]);
        unsigned long theIndex = 0;
        theList->AddValidExtension(theParts[0]);
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory + theParts[1]);
        thePath->AddDirectory(theDirectory, true);
        theList->SetSearchPaths(thePath);
        theList->CreateEntries(1);
        if (theList->FindString(theName, theIndex) == true) {
            theList->ChangeSelectionIndex(theIndex, true, false);
            theList->SetScrollBarCurrentRange(theIndex);
        }
        thePath->Dereference();
        break;
    }
    case EE_SCREENS_TEXT_QUESTION_DELETE:
        gUIManager->DisplayQuestion(theText, true, reinterpret_cast<UIForm*>(static_cast<UIFileListBox*>(theForm->GetControl(inArg))),
                                    EEScreensDeleteSelectedFile, 0);
        break;
    case EE_SCREENS_TEXT_LOAD_POST: {
        std::vector<UString> theParts = Parts(inText, inLength);
        FSFileSpec* theFile = NULL;
        if (EEOpenSaveGameFile(theParts[0], theParts[1], theFile) != kErrorOK)
            return 0;
        gUserInterface->PostLoadSavedGame(*theFile);
        if (theFile)
            delete theFile;
        return 1;
    }
    case EE_SCREENS_TEXT_SAVE_MESSAGE:
        sSaveObject = reinterpret_cast<UIForm*>(static_cast<NEMessage*>(new EEMLoadOrSaveGame(
            theText, TMGetTime(), 0, 0, gCommunications->GetNumberOfCTsProcessedInPhysics(), 0)));
        break;
    case EE_SCREENS_TEXT_SAVE_ACTION: {
        std::vector<UString> theParts = Parts(inText, inLength);
        sSaveObject = reinterpret_cast<UIForm*>(static_cast<EEAction*>(new EEASaveGame(theParts[0], true, theParts[1])));
        break;
    }
    case EE_SCREENS_TEXT_FILE_EXISTS: {
        std::vector<UString> theParts = Parts(inText, inLength);
        FSFileSpec theSpec;
        theSpec.SetFile(theParts[0]);
        theSpec.SetDirectory(gStartupDirectory + theParts[1]);
        if (theSpec.Open(kFileAccessRead) != kErrorOK)
            return 0;
        theSpec.Close();
        return 1;
    }
    case EE_SCREENS_TEXT_SAVE_QUESTION:
        gUIManager->DisplayQuestion(theText, true, sSaveObject, inArg ? EEScreensPostSave : EEScreensSendSave, EEScreensDropSave);
        break;
    case EE_SCREENS_TEXT_LOAD_SAVED_GAME: {
        // the single player load: the saved game validated, the lobby options and a new local server set up for
        // it, its loading started; an invalid file shows the reason (the file is freed on every path)
        std::vector<UString> theParts = Parts(inText, inLength);
        UWideString theMessage;
        FSFileSpec* theFile = NULL;
        if (EEOpenSaveGameFile(theParts[0], theParts[1], theFile) == kErrorOK && theFile->Open(kFileAccessRead) == kErrorOK) {
            if (!EEValidateSaveGameFile(*theFile, theMessage)) {
                gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                delete theFile;
                return 0;
            }
            gWorld.GetLobbyOptions().GetLock()->WriteLock();
            gWorld.GetLobbyOptions().SetDefaults();
            gWorld.GetLobbyOptions().SetLoadType(2);
            gWorld.GetLobbyOptions().SetMapType((const char*)theFile->GetFile());
            gWorld.GetLobbyOptions().GetLock()->WriteUnlock();
            gLoadSavedGameName = theFile->GetFile().GetIndexedSubstring(0, theFile->GetFile().GetLength() - 5);
            delete theFile;
            gGameState.mGameOptions.SetIsMultiplayer(false);
            if (gCommunications)
                delete gCommunications;
            gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
            static_cast<EEServer*>(gCommunications)->StartThreads();
            static_cast<EEServer*>(gCommunications)->SetNumLoadResponses(false);
            gGameStateLoadType = 2;
            static_cast<EEServer*>(gCommunications)->StopPing();
            static_cast<EEServer*>(gCommunications)->SetLoadState(true);
            gCommunications->ProcessLocalUserInput(new EEMStartLoading(1, TMGetTime()), kSlotNone);
            return 1;
        }
        if (theFile != NULL)
            delete theFile;
        return 0;
    }
    case EE_SCREENS_TEXT_SCENARIO_INTRO: {
        // a scenario's header into the scenario intro's form manager (2; the caller opens the form), the scenario
        // the last loaded one; 1: the file is not a scenario, 0: not found
        std::vector<UString> theParts = Parts(inText, inLength);
        EEPSaveGameHeader theHeader;
        FSFileSpec* theFile = NULL;
        if (EEOpenSaveGameFile(theParts[0], theParts[1], theFile) != kErrorOK)
            return 0;
        theHeader.SetMajorVersion(kSaveGameMajorVersion);
        theHeader.SetMinorVersion(kSaveGameMinorVersion);
        if (theHeader.LoadFromFile(*theFile) != kErrorOK) {
            if (theFile)
                delete theFile;
            return 1;
        }
        EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
        theManager->SetMediaStrings(theHeader.GetMediaStrings());
        theManager->SetFile(theFile);
        theManager->SetIsCampaign(false);
        gLoadScenarioName = theFile->GetFile().GetIndexedSubstring(0, theFile->GetFile().GetLength() - 5);
        return 2;
    }
    case EE_SCREENS_TEXT_FORM_EDIT:
        static_cast<UIEditBox*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff))->SetText(theText);
        break;
    }
    return 0;
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

// ---- the settings form's records (settings_form.rs)

// a control's GetCurrentSelection (a combo box's, else a list box's): its result, the selection into outSelection
extern "C" int32_t ee_screenscpp_selection(void* inForm, uint32_t inControl, bool inList, uint32_t* outSelection)
{
    UIControl* theControl = static_cast<UIForm*>(inForm)->GetControl(inControl);
    unsigned long theSelection = 0;
    long theResult = kErrorFailed;
    if (theControl && inList)
        theResult = static_cast<UILBString*>(theControl)->GetCurrentSelection(theSelection);
    else if (theControl)
        theResult = static_cast<UIComboBox*>(theControl)->GetCurrentSelection(theSelection);
    *outSelection = theSelection;
    return theResult;
}

// the fields of EEGameSettings the form reads and writes: (record field, settings member)
#define EE_SCREENS_SETTINGS_FIELDS(X)                                                                                  \
    X(mouse_sensitivity, mMouseSensitivity) X(side_scroll_speed, mSideScrollSpeed)                                     \
    X(grab_scroll_speed, mGrabScrollSpeed) X(sound_volume, mSoundVolume) X(music_volume, mMusicVolume)                 \
    X(max_number_of_sounds, mMaxNumberOfSounds) X(game_bit_depth, mGameBitDepth)                                       \
    X(game_window_height, mGameWindowHeight) X(game_window_width, mGameWindowWidth)                                    \
    X(mip_levels_to_drop, mMIPLevelsToDrop) X(model_detail, mModelDetail) X(effect_detail, mEffectDetail)              \
    X(network_adapter, mNetworkAdapter) X(screen_id, mScreenID) X(terrain_mip_levels_to_drop, mTerrainMIPLevelsToDrop) \
    X(texture_bit_depth, mTextureBitDepth) X(number_of_auto_saves, mNumberOfAutoSaves)                                 \
    X(auto_save_in_milliseconds, mAutoSaveInMilliseconds) X(texture_resolution, mTextureResolution)                    \
    X(ui_scale, mUIScale) X(render_resolution, mRenderResolution) X(window_mode, mWindowMode)                          \
    X(frame_rate_limit, mFrameRateLimit) X(texture_filtering, mTextureFiltering) X(brightness, mBrightness)            \
    X(master_volume, mMasterVolume) X(maximum_zoom_out, mMaximumZoomOut) X(zoom_enabled, mZoomEnabled)                 \
    X(display_clouds, mDisplayClouds) X(display_shadows, mDisplayShadows)                                              \
    X(display_battlefield_ambients, mDisplayBattlefieldAmbients) X(trilinear_filter, mTrilinearFilter)                 \
    X(full_screen, mFullScreen) X(reverb_enabled, mReverbEnabled) X(wait_for_vsync, mWaitForVSync)                     \
    X(ambient_sounds, mAmbientSounds) X(show_blood, mShowBlood) X(music_enabled, mMusicEnabled)                        \
    X(right_hand_mouse_orientation, mRightHandMouseOrientation) X(unit_lighting, mUnitLighting)                        \
    X(display_weather_effects, mDisplayWeatherEffects) X(anti_aliasing, mAntiAliasing)                                 \
    X(mute_in_background, mMuteInBackground)

// the settings (the defaults when inDefaults) into outRecord
extern "C" void ee_screenscpp_settings(bool inDefaults, EeScreensSettingsRecord* outRecord)
{
    const EEGameSettings& theSettings = inDefaults ? EEGamePreferences::sDefaultSettings : gGamePreferences.GetSettings();
#define EE_SCREENS_GET(field, member) outRecord->field = theSettings.member;
    EE_SCREENS_SETTINGS_FIELDS(EE_SCREENS_GET)
#undef EE_SCREENS_GET
}

// a record into the settings
extern "C" void ee_screenscpp_put_settings(const EeScreensSettingsRecord* inRecord)
{
    EEGameSettings& theSettings = gGamePreferences.GetSettings();
#define EE_SCREENS_PUT(field, member) theSettings.member = inRecord->field;
    EE_SCREENS_SETTINGS_FIELDS(EE_SCREENS_PUT)
#undef EE_SCREENS_PUT
}

// FillModes' modes, unsorted (the first screen of index inScreen in the first rasterizer of index inRasterizer that
// has it): up to inCapacity into outModes; their count
extern "C" size_t ee_screenscpp_modes(uint32_t inRasterizer, uint32_t inScreen, EeScreensVideoMode* outModes,
                                      size_t inCapacity)
{
    for (size_t r = 0; r < gDisplayRasterizers.size(); r++) {
        if (gDisplayRasterizers[r].mIndex != inRasterizer)
            continue;
        const std::vector<EEDisplayScreen>& theScreens = gDisplayRasterizers[r].mScreens;
        for (size_t s = 0; s < theScreens.size(); s++) {
            if (theScreens[s].mIndex != inScreen)
                continue;
            const std::vector<EEDisplayMode>& theModes = theScreens[s].mModes;
            for (size_t m = 0; m < theModes.size() && m < inCapacity; m++) {
                outModes[m].width = theModes[m].mMode.mWidth;
                outModes[m].height = theModes[m].mMode.mHeight;
                outModes[m].refresh_rate = theModes[m].mMode.mRefreshRate;
                outModes[m].bit_count = theModes[m].mMode.mBitCount;
            }
            return theModes.size();
        }
    }
    return 0;
}

// the current mode of gRasterizer's screen inScreen
extern "C" void ee_screenscpp_current_mode(uint32_t inScreen, EeScreensVideoMode* outMode)
{
    const GEVideoMode& theMode = gRasterizer->GetScreen(inScreen)->GetCurrentMode();
    outMode->width = theMode.mWidth;
    outMode->height = theMode.mHeight;
    outMode->refresh_rate = theMode.mRefreshRate;
    outMode->bit_count = theMode.mBitCount;
}

// GXGameSize
extern "C" void ee_screenscpp_game_size(uint32_t inPercent, bool inWindowed, uint32_t* outWidth, uint32_t* outHeight)
{
    unsigned long theWidth = 0, theHeight = 0;
    GXGameSize(inPercent, inWindowed, &theWidth, &theHeight);
    *outWidth = theWidth;
    *outHeight = theHeight;
}
