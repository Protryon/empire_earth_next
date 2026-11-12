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
#include "ee/EEUIECampaignSelectScreenEvents.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEMouse.h"
#include "ee/EEGameSound.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/RTSGraphics.h"
#include "ee/EEDisplayDevices.h"
#include "platform/PlatWindow.h"
#include "rust/ee_screens.h"

void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);   // lle/SEMixer.h
unsigned long EEMenuWidth();   // Empire Earth.cpp
unsigned long EEMenuHeight();

UString gMultiplayerString("");             // 0x562fd8 (EEUserInterface.cpp keeps the WON password in it)
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

UIEvent* EECreateScreenEvent(unsigned long inEvent)
{
    switch (inEvent) {
    case 0:
        return NULL;
    case EE_SCREENS_CPP_SCENARIOS_OPEN:
        return new EEUIECSSEFormOpen;
    case EE_SCREENS_CPP_SCENARIOS_CLOSE:
        return new EEUIECSSEFormClose;
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
    EEUIManager* theManager = static_cast<EEUIManager*>(theForm->GetManager());
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
    }
    return 0;
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
        return EECampaignFormManager::GetBuiltInCampaign(theText) != 0;
    case EE_SCREENS_TEXT_MESSAGE_NONE_FOUND:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensNoCustomCampaigns, true);
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
