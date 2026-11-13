// C:\EEX\Empire Earth\User Interface Thread\EEUIManager.cpp (file name inferred: no __FILE__; RTTI class
// EEUIManager, vtable 0x44ae90). The game's UI manager: 0x271319..0x2753b1 (EEUIMiniMap.cpp's chains follow). The
// ctor (registers the event factories below and the form-event factory), ??_G, dtor, then the factories in address
// order, each followed by the ??_G bodies of the events it creates first, then the overrides, the message boxes,
// StartScenarioEditor and the callback vectors' COMDATs. The factories have provisional names (by the forms they
// serve); their case ids and classes come from emulating the original dispatch. Unit flags: the exe module set.
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
#include "lle/UILayoutUtility.h"
#include "lle/UIControlsTable.h"
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
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UITLScrolling.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EmpireEarth.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphics.h"
#include "ee/EETileCoordinate.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEUIBSavedGroup.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUIFMGMainGroup.h"
#include <locale.h>
#include "lle/UICheckBox.h"
#include "lle/UIStatusBar.h"
#include "lle/UIBlinkingButton.h"
#include "lle/SECD.h"
#include "lle/SEConductor.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EEWonNet.h"
#include "ee/EERenderObject.h"
#include "ee/EEROUserInterface.h"
#include "ee/EEGameOptions.h"
#include "ee/EECPEngine.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEGameState.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESaveLoad.h"
#include "ee/EESMDialogMessage.h"
#include "lle/UIFormManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEUIEWonNetServerSelectEvents.h"
#include "ee/EEUIFError.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEABootClientFromCanceledGame.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EEUIECampaignEditorEvents.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"   // ee_screens_menus_event_for (ee_screens menus.rs)
#include "ee/EEUIEProcessTimerExpired.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEUIEWonNetArenaRoomEvents.h"
#include "ee/EEUIEWonNetModifyUserAccountEvents.h"
#include "ee/EEUIEWonNetStatsEvents.h"
#include "lle/UIBitmap.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIBGenericBackground.h"
#include "ee/EEUIManagerEvents.h"
#include "ee/EEFormEventID.h"
#include "ee/EEControlEventID.h"


class UIFileListBox;
extern bool& gGameStateIsMultiplayer;        // 0x52fa88 (cleared when the scenario editor starts) (provisional name)
extern bool& gLoadingSavedGame;              // 0x5318d8
extern UString gSavedGameExtension;         // 0x530ba8
extern unsigned long& gTexturePixelDepth;    // 0x5308b4
extern FSPath* gDataPath;                   // 0x531908
void EEPostF9Key();                         // 0x27a7f5

// the event factories (UIManager's control-event callbacks, then its form-event callback)
UIEvent* EECreateGameSettingsEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateFormEvent(UIManager* inManager, unsigned long inID);
UIEvent* EECreateSaveLoadEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateDebugSoundEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateCampaignEditorEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateChatLogEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateCivilizationBuilderEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateMainGameEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateMainScreenEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreatePostGameSummaryEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateScenarioIntroEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateScenarioLoadEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateSinglePlayerLoadEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateWonNetServerSelectEvent(UIForm* inForm, unsigned long inID);

#include "lle/GELine.h"
#include "lle/UIControlsTable.h"
#include "lle/UIBackgroundTable.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIFMGErrorLog.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEUICPortrait.h"
#include "ee/EEUICChatLine.h"
#include "ee/EEUICUnitIconBar.h"
#include "ee/EEUICPing.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEUICWonNetStatBox.h"
#include "lle/UGraph.h"
#include "lle/UIGraph.h"
#include "lle/UILineGraph.h"
#include "ee/EEUIPGSLineGraph.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x671319
// Creates the game's UI manager: registers the control and form event factories (the game settings one
// twice), loads the UI database and finds the main game form
EEUIManager::EEUIManager(GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                         const UString& inUnused)
    : UIManager(inRasterizer, gSoundManager, inNumberOfSurfaces, gTexturePixelDepth, gResource, inScreen)
{
    this->RegisterControlEventCallback(EECreateCampaignEditorEvent);
    this->RegisterControlEventCallback(EECreateChatLogEvent);
    this->RegisterControlEventCallback(EECreateCivilizationBuilderEvent);
    this->RegisterControlEventCallback(EECreateGameSettingsEvent);
    this->RegisterControlEventCallback(EECreateSaveLoadEvent);
    this->RegisterControlEventCallback(EECreateMainGameEvent);
    this->RegisterControlEventCallback(EECreateMainScreenEvent);   // + single player, play campaign (Rust's table)
    this->RegisterControlEventCallback(EECreatePostGameSummaryEvent);
    this->RegisterControlEventCallback(EECreateScenarioIntroEvent);
    this->RegisterControlEventCallback(EECreateScenarioLoadEvent);
    this->RegisterControlEventCallback(EECreateSinglePlayerLoadEvent);
    this->RegisterControlEventCallback(EECreateGameSettingsEvent);
    this->RegisterControlEventCallback(EECreateWonNetServerSelectEvent);
    this->RegisterControlEventCallback(EECreateDebugSoundEvent);
    this->RegisterFormEventCallback(EECreateFormEvent);
    this->LoadDatabase(gDataPath, UString());
    // port: the settings' texture resolution combo box (ee_screens settings_form.rs), in the video tab's right column
    // below the weather effects check box; a copy of the video card combo box (id 151) with the strings 33100..33102
    UIControlsTable::AddControlLike(this, 151, kTextureResolutionControlID, kGameSettingsTextureResolutionComboBox, 1,
                                    0.2473f, 0.4614f, 0.325f, 0.325f + 0.025f + 3 * 0.022f, kTextureResolutionOriginalString,
                                    kTextureResolutionOriginalString + 2, kTextureResolutionHelpString);
    // port: the modern display settings in the video tab's top block (where the video card, the 3D acceleration and
    // the resolution list were; those are hidden), the zoom-out in the interface tab, the master volume, the output
    // device and the background mute in the audio tab. Combo boxes are copies of the video card's (id 151): their
    // lines are their strings, the rectangles include the open list.
    struct PortControl { long mTemplate, mIndex; unsigned long mGroup; float mL, mR, mT, mLines; long mBegin, mEnd, mHelp; };
    static const PortControl kPortControls[] = {
        {151, kGameSettingsWindowModeComboBox, 1, 0.2473f, 0.4614f, 0.160f, 2, 33110, 33111, 33150},
        {151, kGameSettingsUIScaleComboBox, 1, 0.2473f, 0.4614f, 0.215f, 8, 33112, 33119, 33151},
        {151, kGameSettingsRenderResolutionComboBox, 1, 0.2473f, 0.4614f, 0.270f, 4, 33120, 33123, 33152},
        {151, kGameSettingsFrameRateComboBox, 1, 0.5400f, 0.7540f, 0.160f, 4, 33124, 33127, 33153},
        {151, kGameSettingsTextureFilteringComboBox, 1, 0.5400f, 0.7540f, 0.215f, 3, 33128, 33130, 33154},
        {151, kGameSettingsBrightnessComboBox, 1, 0.5400f, 0.7540f, 0.270f, 7, 33131, 33137, 33155},
        {151, kGameSettingsMaximumZoomOutComboBox, 3, 0.2473f, 0.4614f, 0.340f, 9, 33138, 33146, 33156},
        {182, kGameSettingsMasterVolumeLabel, 2, 0.2473f, 0.4614f, 0.560f, -1, 33147, 0, 33157},
        {183, kGameSettingsMasterVolumeSlider, 2, 0.2473f, 0.4614f, 0.587f, -1, 0, 0, 33157},
        {151, kGameSettingsOutputDeviceComboBox, 2, 0.5400f, 0.7540f, 0.587f, 6, 33148, 33148, 33158},
        {804, kGameSettingsMuteInBackgroundCheckBox, 2, 0.5400f, 0.7540f, 0.535f, -1, 33149, 0, 33159},
        {151, kGameSettingsQualityComboBox, 1, 0.3130f, 0.5270f, 0.490f, 6, 33160, 33165, 33166},
    };
    for (size_t i = 0; i < sizeof(kPortControls) / sizeof(kPortControls[0]); i++) {
        const PortControl& c = kPortControls[i];
        float theBottom = c.mLines >= 0 ? c.mT + 0.025f + c.mLines * 0.022f : c.mT + 0.0245f;
        UIControlsTable::AddControlLike(this, c.mTemplate, kTextureResolutionControlID + 1 + (long)i, c.mIndex, c.mGroup,
                                        c.mL, c.mR, c.mT, theBottom, c.mBegin, c.mEnd, c.mHelp);
    }
    // port: the lobby's player column narrower (the name combo boxes, form indices 30..45, and the name labels,
    // 46..61: 0.0587..0.2532 -> ..0.2312) and the columns right of it moved left by as much (the team, citizens,
    // bonus and ping headers 3..6; per slot the team button and label, the colour button and label, the citizens
    // combo and label and the ping status, 62..173), so that the engine column below ends before the game options'
    // frame (about 0.508) instead of under it
    const float kLobbyPlayerShrink = 0.022f;
    UIControlsTable::MoveControlsHorizontally(kFormLobby, 30, 61, 0.0f, -kLobbyPlayerShrink);
    UIControlsTable::MoveControlsHorizontally(kFormLobby, 3, 6, -kLobbyPlayerShrink, -kLobbyPlayerShrink);
    UIControlsTable::MoveControlsHorizontally(kFormLobby, 62, 173, -kLobbyPlayerShrink, -kLobbyPlayerShrink);
    // port: the lobby's engine column (EELobbyControl kLobbyEngine*; ee/EECPEngine.h) where the ping column is (a
    // computer row has no ping), widened into the gap before the game options: the header a copy of the ping label
    // (2507), per slot a combo box like the citizens combo (2629; its lines the strings Classic and Random, then the
    // AI manifest's names, which the lobby appends: ee_screens lobby_form.rs fill_engine_box) and a label like the
    // citizens label (2645), on the slot rows (the database's rows: 0.1043 + slot * 0.0391); moved left with the
    // columns above (0.4282..0.5200 in the database's places). The open list is as tall as its lines (about 0.034 of
    // the height each in this lobby), up to 8; more scroll.
    {
        const float kLeft = 0.4282f - kLobbyPlayerShrink, kRight = 0.5200f - kLobbyPlayerShrink;
        const float kRowTop = 0.10430248f, kRowStep = 0.03911343f;
        const float kRowHeight = 0.02477184f;
        const float kListLine = 0.034f;
        const unsigned long kListLinesMax = 8;
        unsigned long theLines = EECPEngineChoiceCount();
        if (theLines > kListLinesMax)
            theLines = kListLinesMax;
        UIControlsTable::AddControlLike(this, 2507, kLobbyEngineControlID, kLobbyEngineHeader, 0, kLeft, kRight,
                                        0.058670145f, 0.102998696f, kLobbyEngineHeaderString, 0, kLobbyEngineHelpString);
        for (long theSlot = 0; theSlot < 16; theSlot++) {
            float theTop = kRowTop + theSlot * kRowStep;
            UIControlsTable::AddControlLike(this, 2629, kLobbyEngineControlID + 1 + theSlot, kLobbyEngineCombo + theSlot, 1,
                                            kLeft, kRight, theTop, theTop + 0.025f + theLines * kListLine,
                                            kLobbyEngineClassicString, kLobbyEngineRandomString,
                                            kLobbyEngineHelpString);
            UIControlsTable::AddControlLike(this, 2645, kLobbyEngineControlID + 17 + theSlot, kLobbyEngineLabel + theSlot, 1,
                                            kLeft, kRight, theTop, theTop + kRowHeight, kLobbyEngineClassicString, 0,
                                            kLobbyEngineHelpString);
        }
    }
    this->mMainGameForm = static_cast<EEUIFMainGame*>(this->GetFormByID(kFormMainGame));
    this->mInScenarioEditor = false;
    this->mScenarioEditorForm = NULL;
    this->mB13D = false;
    this->SetMusicVolume(!gGamePreferences.mSettings.mMusicEnabled ? 0.0f : gGamePreferences.mSettings.mMusicVolume);
    this->mU138 = 0;
    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_SL_ACTIVATE);   // (ee_screens save_load.rs)
    this->GetFormByID(kFormSaveGame)->SetFormActivateEvent(theEvent);
    theEvent->Dereference();
}

// SYNTHETIC: EXE 0x671552
// (EEUIManager's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x67156e
// Destroys the UI manager and its scenario editor form
EEUIManager::~EEUIManager()
{
    if (this->mScenarioEditorForm != NULL)
        delete this->mScenarioEditorForm;
    this->mMainGameForm = NULL;
}

// FUNCTION: EXE 0x671598
// Creates the game settings form's event for a control id; NULL for other ids (the codes are Rust's: ee_screens
// settings_form.rs)
UIEvent* EECreateGameSettingsEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_settings_event_for(inID));
}

// FUNCTION: EXE 0x671a10
// Creates the event a form fires when it opens, closes or activates; NULL for other ids
UIEvent* EECreateFormEvent(UIManager* inManager, unsigned long inID)
{
    UIEvent* theEvent = NULL;
    switch (inID) {
    case kFormEventGameSettingsOpen:
        break;
    case kFormEventMainScreenOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_MAIN_OPEN);
        break;
    case kFormEventChatFormOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_CH_OPEN);
        break;
    case kFormEventChatFormClose:
        theEvent = EECreateScreenEvent(EE_SCREENS_CH_CLOSE);
        break;
    case kFormEventChatMLBPressed:
    case kFormEventChatMRBPressed:
        theEvent = EECreateScreenEvent(EE_SCREENS_CH_CANCEL);
        break;
    case kFormEventWNSFormOpen:
        theEvent = new EEUIEWNSOpen;
        break;
    case kFormEventScenarioIntroOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_SI_OPEN);
        break;
    case kFormEventScenarioIntroClose:
        theEvent = EECreateScreenEvent(EE_SCREENS_SI_CLOSE);
        break;
    case kFormEventScenarioLoadOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_SCL_OPEN);
        break;
    case kFormEventSinglePlayerLoadOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_SPL_OPEN);
        break;
    case kFormEventSSFormOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_SS_OPEN);   // (ee_screens splash.rs)
        break;
    case kFormEventPGSOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_PGS_OPEN);
        break;
    case kFormEventPGSClose:
        theEvent = EECreateScreenEvent(EE_SCREENS_PGS_CLOSE);
        break;
    case kFormEventDSOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_DS_OPEN);
        break;
    case kFormEventCEOpen:
        theEvent = new EEUIECEOpen;
        break;
    case kFormEventCEClose:
        theEvent = new EEUIECEClose;
        break;
    case kFormEventChatLogOpen:
        theEvent = EECreateScreenEvent(EE_SCREENS_CL_OPEN);
        break;
    case kFormEventChatLogMLBPressed:
    case kFormEventChatLogMRBPressed:
        theEvent = EECreateScreenEvent(EE_SCREENS_CL_MOUSE_PRESSED);
        break;
    }
    return theEvent;
}

// SYNTHETIC: EXE 0x671daf
// (EEUIEWNSOpen's scalar deleting destructor, after its first user)


// SYNTHETIC: EXE 0x671eb1
// (EEUIECEOpen's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x671edc
// (EEUIECEClose's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x671f5d
// Creates the save/load game form's event for a control id; NULL for other ids (the codes are Rust's: ee_screens
// save_load.rs; made with the form, whose screen and editor decide the saved games' or the scenarios' kind). The
// load form's Cancel opens the form it was made for.
UIEvent* EECreateSaveLoadEvent(UIForm* inForm, unsigned long inID)
{
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    if (inID == kControlEventCancelLoad)
        return new EEUIECancel(!gGameScreenActive ? static_cast<UIForm*>(theManager->mScenarioEditorForm)
                                                  : theManager->mMainGameForm);
    return EECreateScreenEvent(ee_screens_save_load_event_for(inID, gGameScreenActive, theManager->mInScenarioEditor));
}

// FUNCTION: EXE 0x6724af
// Opens the form the cancel event was made for (the save/load screens' cancel)
// (EEUIECancel::ProcessEvent, inline in the class)

// SYNTHETIC: EXE 0x6724c0
// (EEUIECancel's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x6724eb
// Creates the debug sound form's event for a control id; NULL for other ids (the codes are Rust's: ee_screens
// debug_sound.rs)
UIEvent* EECreateDebugSoundEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_debug_sound_event_for(inID));
}

// FUNCTION: EXE 0x6726b3
// Creates the campaign editor's event for a control id; NULL for other ids
UIEvent* EECreateCampaignEditorEvent(UIForm* inForm, unsigned long inID)
{
    UIEvent* theEvent = NULL;
    if (inID >= kControlEventCEAddScenario && inID <= kControlEventCECampaignNew) {
        switch (inID) {
        case kControlEventCEAddScenario:
            return new EEUIECEAddScenario;
        case kControlEventCEDeleteCampaign:
            return new EEUIECECampaignDelete;
        case kControlEventCECampaignNameChanged:
            return new EEUIECECampaignNameChanged;
        case kControlEventCECampaignSave:
            return new EEUIECECampaignSave;
        case kControlEventCECampaignSelected:
            return new EEUIECECampaignSelected;
        case kControlEventCEMoveScenarioUp:
            return new EEUIECEMoveScenarioUp;
        case kControlEventCEMoveScenarioDown:
            return new EEUIECEMoveScenarioDown;
        case kControlEventCERemoveScenario:
            return new EEUIECERemoveScenario;
        case kControlEventCEMainMenu:
            return new EEUIECEMainScreen;
        case kControlEventCEStartScenarioEditor:
            return new EEUIECEScenarioEditorOpen;
        case kControlEventCEPasswordEntered:
        case kControlEventCEPasswordOKButtonHit:
            return new EEUIECEPasswordEntered;
        case kControlEventCECampaignNew:
            return new EEUIECECampaignNew;
        }
    }
    return theEvent;}

// SYNTHETIC: EXE 0x6728ad
// (EEUIECEAddScenario's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x6728d8
// (EEUIECERemoveScenario's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672903
// (EEUIECEMoveScenarioUp's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x67292e
// (EEUIECEMoveScenarioDown's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672959
// (EEUIECECampaignNameChanged's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672984
// (EEUIECECampaignSave's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x6729af
// (EEUIECECampaignSelected's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x6729da
// (EEUIECECampaignNew's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672a05
// (EEUIECEPasswordEntered's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672a30
// (EEUIECECampaignDelete's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672a5b
// (EEUIECEMainScreen's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x672a86
// (EEUIECEScenarioEditorOpen's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x672ab1
// Creates the chat log form's event for a control id; NULL for other ids
UIEvent* EECreateChatLogEvent(UIForm* inForm, unsigned long inID)
{
    UIEvent* theEvent = NULL;
    switch (inID) {
    case kControlEventChatLogClose:
        theEvent = EECreateScreenEvent(EE_SCREENS_CL_CLOSE);
        break;
    }
    return theEvent;
}

// FUNCTION: EXE 0x672ade
// Creates the civilization builder's event for a control id; NULL for other ids
UIEvent* EECreateCivilizationBuilderEvent(UIForm* inForm, unsigned long inID)
{
    UIEvent* theEvent = NULL;
    switch (inID) {
    case kControlEventCBSaveGameNameSelected:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_SAVE_NAME_SELECTED);
        break;
    case kControlEventCBCancelNewCivDialog:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_CANCEL_NAME);
        break;
    case kControlEventCBShowLoadCivDialog:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_SHOW_LOAD);
        break;
    case kControlEventCBCancelLoadCivDialog:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_CANCEL_LOAD);
        break;
    case kControlEventCBShowSaveCivDialog:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_SHOW_SAVE);
        break;
    case kControlEventCBCancelSaveCivDialog:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_CANCEL_SAVE);
        break;
    case kControlEventCBPurchasedUpgradeSelected:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_PURCHASED_SELECTED);
        break;
    case kControlEventCBPotentialUpgradeSelected:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_POTENTIAL_SELECTED);
        break;
    case kControlEventCBUpgradePurchased:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_PURCHASE);
        break;
    case kControlEventCBUpgradeReturned:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_RETURNED);
        break;
    case kControlEventCBUpdateControls:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_UPDATE_CONTROLS);
        break;
    case kControlEventCBCancelChanges:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_CANCEL_CHANGES);
        break;
    case kControlEventCBAcceptChanges:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_CHANGES);
        break;
    case kControlEventCBShowChangeNameDialog:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_SHOW_CHANGE_NAME);
        break;
    case kControlEventCBSaveCivEnterPressed:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_SAVE);
        break;
    case kControlEventCBAcceptLoadCivilization:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_LOAD);
        break;
    case kControlEventCBAcceptChangeName:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_CHANGE_NAME);
        break;
    case kControlEventCBDisplayDeleteCivDialogInLoad:
    case kControlEventCBDisplayDeleteCivDialogInSave:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_DELETE);
        break;
    case kControlEventCBAcceptChangeNameEnterPressed:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_CHANGE_NAME);
        break;
    case kControlEventCBAcceptLoadCivilizationDoubleClicked:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_LOAD);
        break;
    case kControlEventCBAcceptSaveCivilization:
        theEvent = EECreateScreenEvent(EE_SCREENS_CB_ACCEPT_SAVE);
        break;
    }
    return theEvent;
}

// FUNCTION: EXE 0x672de6
// Creates the main game form's event for a control id; NULL for other ids
UIEvent* EECreateMainGameEvent(UIForm* inForm, unsigned long inID)
{
    UIEvent* theEvent = NULL;
    switch (inID) {
    case kControlEventSRMMainGameStatistics:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_STATISTICS);
        break;
    case kControlEventSRMMainGameDiplomacy:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_DIPLOMACY);
        break;
    case kControlEventSRMMainGameChat:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_CHAT);
        break;
    case kControlEventSRMMainGameOptions:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_OPTIONS);
        break;
    case kControlEventSRMLargeCivBuilderAlert:
    case kControlEventSRMMainGameCivBuilder:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_CIV_BUILDER);
        break;
    case kControlEventSRMLargeScenBriefingAlert:
    case kControlEventSRMGameInfoScenarioIntroduction:
        theEvent = EECreateScreenEvent(EE_SCREENS_GO_DISPLAY_INTRO);
        break;
    case kControlEventSRMTempBoxLoadUnit:
        theEvent = new EEUIELoadUnit;
        break;
    case kControlEventSRMIdleCitizen:
        theEvent = EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCitizen, true, false, false));
        break;
    case kControlEventSRMIdleMilitary:
        theEvent = EECreateScreenEvent(ee_screens_hot_key_unit_selection(0, false, true, false));
        break;
    case kControlEventSRMFighterRallyTimer:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_RALLY_FIGHTER);
        break;
    case kControlEventSRMBomberRallyTimer:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_RALLY_BOMBER);
        break;
    case kControlEventSRMAtomicRallyTimer:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_RALLY_ATOMIC);
        break;
    case kControlEventSRMChatDialogFinished:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_END_SCENARIO_DIALOG);
        break;
    case kControlEventSRMHiddenUnitToggle:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_TOGGLE_HIDDEN);
        break;
    case kControlEventSRMFlare:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FLARE);
        break;
    case kControlEventSRMSetScoutBehavior:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_SCOUT);
        break;
    case kControlEventSRMSetStandGroundBehavior:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_STAND_GROUND);
        break;
    case kControlEventSRMSetGuardBehavior:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_GUARD);
        break;
    case kControlEventSRMSetAggressiveBehavior:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE);
        break;
    case kControlEventSRMSetFormationShortLine:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 0);
        break;
    case kControlEventSRMSetFormationLongLine:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 1);
        break;
    case kControlEventSRMSetFormationLooseLine:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 2);
        break;
    case kControlEventSRMSetFormationStaggeredLine:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 3);
        break;
    case kControlEventSRMSetFormationDeepWedge:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 4);
        break;
    case kControlEventSRMSetFormationShallowWedge:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 5);
        break;
    case kControlEventSRMSetFormationInwardCircle:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 6);
        break;
    case kControlEventSRMSetFormationOutwardCircle:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_FORMATION + 7);
        break;
    case kControlEventSRMDisplayChatLog:
        theEvent = EECreateScreenEvent(EE_SCREENS_MG_PREVIOUS_MESSAGES);
        break;
    }
    return theEvent;
}

// FUNCTION: EXE 0x673345
// Reloads the unit data through the main game form's unit editing hack (debug load-unit event)
// (EEUIELoadUnit::ProcessEvent, inline in the class)

// SYNTHETIC: EXE 0x673359
// (EEUIELoadUnit's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x6734c5
// Creates the main screen's, the editors' sub-menu's, the single player form's and the play campaign form's event for
// a control id (the menus are Rust's whole: ee_screens menus.rs; the original's single player and play campaign
// factories, 0x674200 and 0x673836, folded in); NULL for other ids
UIEvent* EECreateMainScreenEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_menus_event_for(inID));
}

// FUNCTION: EXE 0x673a89
// Creates the post-game summary's event for a control id; NULL for other ids (ee_screens post_game.rs)
UIEvent* EECreatePostGameSummaryEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_post_game_event_for(inID));
}

// FUNCTION: EXE 0x673e7d
// Creates the scenario editor form on first use
void EEUIManager::CreateScenarioEditor()
{
    if (this->mScenarioEditorForm == NULL)
        this->mScenarioEditorForm = new EEUIFScenarioEditor(this);
}

// FUNCTION: EXE 0x673eac
// Creates the scenario intro form's event for a control id; NULL for other ids (ee_screens scenario_screens.rs)
UIEvent* EECreateScenarioIntroEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_scenario_intro_event_for(inID));
}

// FUNCTION: EXE 0x67406e
// Creates the scenario load form's event for a control id; NULL for other ids (ee_screens save_load.rs)
UIEvent* EECreateScenarioLoadEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_scenario_load_event_for(inID));
}

// FUNCTION: EXE 0x6743d2
// Creates the single player load form's event for a control id; NULL for other ids (ee_screens save_load.rs)
UIEvent* EECreateSinglePlayerLoadEvent(UIForm* inForm, unsigned long inID)
{
    return EECreateScreenEvent(ee_screens_sp_load_event_for(inID));
}

// FUNCTION: EXE 0x674564
// Creates the game's framed generic background for a background table entry (flags from its attributes)
UIBackground* EEUIManager::CreateUserDefinedBackground(const UIBackgroundTable& inTable)
{
    GETextureDescription theDescription;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mPixelDepth = this->GetUITextureBitDepth();
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mCreateMIPMaps = true;
    theDescription.mHasAlpha = inTable.GetHasAlpha();
    UIBackground* theResult = NULL;
    if (inTable.GetBackgroundType() == kBackgroundGeneric) {
        UIForm* theForm = this->GetFormByID(inTable.GetFormID());
        if (theForm != NULL) {
            unsigned long theFlags = (inTable.GetAttribute1() == true ? 1 : 0) |
                                     (inTable.GetAttribute2() == true ? 2 : 0) |
                                     (inTable.GetAttribute3() == true ? 4 : 0);
            theResult = new EEUIBGenericBackground(inTable.GetBackgroundID(), inTable.GetTextureFile(),
                theDescription, inTable.GetLayout(), theForm, inTable.GetDrawPriority(),
                inTable.GetColor(), inTable.GetVisible(), inTable.GetOpacity(), 1.0f, 1.0f, theFlags);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x674647
// Creates the game's own controls from the UI database (mini map, status buttons, icon bars, WON lists ...)
// The result lives in one variable kept in memory (the original's [ebp+8]): the line graph case
// works through it (casts), which is what spills it. Case bodies in the original's layout order. The status bar
// takes a non-const UILayoutUtility* while the table exports only the const GetLayout (hence the cast).
// The games box's `new 0x19c` needs its reserved tail (EEUICWonNetGamesBox.h; docs/LOST_DEBUG.md).
UIControl* EEUIManager::CreateUserDefinedControl(UIControlsTable& inTable)
{
    UIControl* theControl;
    std::vector<UWideString> theStrings;
    GERectangle theRectangle, theParent;
    UIForm* theForm = this->GetFormByID(inTable.GetFormID());
    GEWindow* theWindow = this->mRasterizer->GetWindow(this->mScreen);
    float theExclusive = theForm->GetUseExclusiveCoords() ? 0.0f : 1.0f;
    theParent.SetLeft(0.0f);
    theParent.SetTop(0.0f);
    theParent.SetRight((float)theWindow->GetWidth() - theExclusive);
    theParent.SetBottom((float)theWindow->GetHeight() - theExclusive);
    inTable.GetLayout().CalculateRectangle(theParent, theRectangle, theForm->GetUseExclusiveCoords());
    inTable.SetupStringArray(this, theStrings);
    switch (inTable.GetControlType()) {
    case kDbControlMiniMap:
        theControl = new EEUIMiniMap(theRectangle, theForm, inTable.GetFormIndex(), inTable.GetID(), 0);
        break;
    case kDbControlStatusButton:
        theControl = new EEUIFMGStatusButton(this, theRectangle, inTable);
        break;
    case kDbControlErrorLog:
        theControl = new EEUIFMGErrorLog(theRectangle, theForm, inTable.GetFont(), std::vector<UWideString>(),
                                   inTable.GetFormIndex(), inTable.GetID());
        break;
    case kDbControlAttribute:
        theControl = new EEUIFMGAttribute(inTable.GetFont(), theForm, theRectangle, inTable.GetFormIndex(),
                                    inTable.GetID(), inTable.GetTextForeground(), false,
                                    inTable.GetBorderThickness());
        break;
    case kDbControlMainGameStatusBar:
        theControl = new EEUIFMGStatusBar(inTable.GetFormID(), inTable.GetFormIndex(),
                                    (UILayoutUtility*)&inTable.GetLayout(), 800, 600, theRectangle, theForm,
                                    false, true, inTable.GetLoopMovie());
        break;
    case kDbControlSelectUnits:
        theControl = new EEUICSelectUnits(theForm, inTable.GetID(), inTable.GetFormIndex(),
                                    this->mMainGameForm->mMouseClick, this->mMainGameForm->mLasso);
        break;
    case kDbControlCommandUnits:
        theControl = new EEUICCommandUnits(theForm, inTable.GetID(), inTable.GetFormIndex(), this->mMainGameForm->mMouseClick);
        break;
    case kDbControlPortrait:
        theControl = new EEUICPortrait(theForm, theRectangle, inTable.GetID(), inTable.GetFormIndex(), false);
        break;
    case kDbControlChatLine:
        theControl = new EEUICChatLine(theRectangle, inTable, theForm);
        break;
    case kDbControlCBListBox:
        inTable.SetupFont(this, theRectangle, true);
        theControl = new EECBUIListBox(inTable, inTable.GetID(), theStrings, inTable.GetFont(), theRectangle,
                                 theForm, inTable.GetFormIndex(), inTable.GetBorderThickness(),
                                 inTable.GetStartChecked());
        break;
    case kDbControlWonNetRoomsBox:
        theControl = new EEUICWonNetRoomsBox(inTable.GetID(), inTable.GetFont(), theRectangle.GetTopLeft(),
            (unsigned long)theRectangle.GetWidth(), (unsigned long)theRectangle.GetHeight(),
            theForm, inTable.GetFormIndex(), inTable.GetBorderThickness());
        break;
    case kDbControlWonNetGamesBox:
        theControl = new EEUICWonNetGamesBox(inTable.GetID(), inTable.GetFont(), theRectangle.GetTopLeft(),
            (unsigned long)theRectangle.GetWidth(), (unsigned long)theRectangle.GetHeight(),
            theForm, inTable.GetFormIndex(), inTable.GetBorderThickness());
        break;
    case kDbControlWonNetStatBox:
        theControl = new EEUICWonNetStatBox(inTable.GetID(), inTable.GetFont(), theRectangle.GetTopLeft(),
            (unsigned long)theRectangle.GetWidth(), (unsigned long)theRectangle.GetHeight(),
            theForm, inTable.GetFormIndex(), inTable.GetBorderThickness());
        break;
    case kDbControlSavedGroup:
        theControl = new EEUIBSavedGroup(inTable, theRectangle, theForm);
        break;
    case kDbControlUnitIconBar:
        theControl = new EEUICUnitIconBar(inTable, theRectangle, theForm);
        break;
    case kDbControlPGSLineGraph:
        theControl = new EEUIPGSLineGraph(inTable.GetID(), theRectangle, theForm, inTable.GetFormIndex(),
                                          inTable.GetBorderThickness());
        static_cast<EEUIPGSLineGraph*>(theControl)->SetFont(inTable.GetFont());
        static_cast<EEUIPGSLineGraph*>(theControl)->SetFontColor(inTable.GetTextForeground());
        break;
    case kDbControlPing:
        theControl = new EEUICPing(inTable.GetID(), theRectangle.GetTopLeft(), theForm, inTable.GetFormIndex());
        break;
    default:
        theControl = NULL;
        break;
    }
    return theControl;
}

// FUNCTION: EXE 0x674c94
// Creates the main game form as the game's own class; other forms as the base manager does
UIForm* EEUIManager::CreateForm(const UIDbDataForms& inData)
{
    if (inData.mFormID != kFormMainGame)
        return UIManager::CreateForm(inData);
    return new EEUIFMainGame(this, inData);
}

// FUNCTION: EXE 0x674ccf
// Creates the WON server select form's cancel event; NULL for other ids
UIEvent* EECreateWonNetServerSelectEvent(UIForm* inForm, unsigned long inID)
{
    UIEvent* theEvent = NULL;
    switch (inID) {
    case kControlEventWNSCancel:
        theEvent = new EEUIEWNSCancel;
        break;
    }
    return theEvent;
}

// SYNTHETIC: EXE 0x674cf9
// (EEUIEWNSCancel's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x674d24
// Deletes the scenario editor form
void EEUIManager::DeleteScenarioEditor()
{
    if (this->mScenarioEditorForm != NULL) {
        delete this->mScenarioEditorForm;
        this->mScenarioEditorForm = NULL;
    }
}

// FUNCTION: EXE 0x674d40
// Shows a new message box asking for a text or password
void EEUIManager::DisplayPasswordPrompt(const UWideString& inMessage, unsigned long inAlignment, EEUIFPasswordCallback inCallback,
                                        bool inFlag)
{
    (new EEUIFError(this))->DisplayPassword(inMessage, inAlignment, inCallback, inFlag);
}

// FUNCTION: EXE 0x674d79
// Shows a new message box with only a Cancel button
void EEUIManager::DisplayCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                EEUIFErrorCallback inCallback, UIForm* inCancel)
{
    (new EEUIFError(this))->DisplayCancel(inMessage, inAlignment, inForm, inCallback, inCancel);
}

// FUNCTION: EXE 0x674db5
// Shows a new message box with an optional OK button
void EEUIManager::DisplayMessage(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                 EEUIFErrorCallback inCallback, bool inFlag)
{
    (new EEUIFError(this))->DisplayMessage(inMessage, inAlignment, inForm, inCallback, inFlag);
}

// FUNCTION: EXE 0x674df1
// Shows a new Yes/No message box
void EEUIManager::DisplayQuestion(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inYes,
                                  EEUIFErrorCallback inNo)
{
    (new EEUIFError(this))->DisplayQuestion(inMessage, inAlignment, inForm, inYes, inNo);
}

// FUNCTION: EXE 0x674e2d
// Shows a new Yes/No/Cancel message box
void EEUIManager::DisplayYesNoCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                     EEUIFErrorCallback inYes, EEUIFErrorCallback inNo)
{
    (new EEUIFError(this))->DisplayYesNoCancel(inMessage, inAlignment, inForm, inYes, inNo);
}

// FUNCTION: EXE 0x674e69
// Starts the scenario editor: a local server with a human in slot 0 and computers in slots 1..15, then loading
void EEUIManager::StartScenarioEditor()
{
    UWideString theHumanName;
    UWideString theComputerName;
    EEGameOptionsPlayer theHuman;
    EEGameOptionsPlayer theComputer;
    gWorld.GetLobbyOptions().GetLock()->WriteLock();
    this->mInScenarioEditor = true;
    gGameStateIsMultiplayer = false;
    if (gCommunications != NULL)
        delete gCommunications;
    gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
    static_cast<EEServer*>(gCommunications)->StartThreads();
    static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0;
    gWorld.GetLobbyOptions().mMapSize = 1;
    gWorld.GetLobbyOptions().mLoadType = 3;
    gWorld.GetLobbyOptions().mEpochStart = kTechEpochPrehistoric;
    gWorld.GetLobbyOptions().mVariant = 0;
    gResource->GetString(IDS_SE_HUMAN, theHumanName);
    theHuman.mColor = 0;
    theHuman.mIsLocal = true;
    theHuman.mName = theHumanName;
    theHuman.mSlot = 0;
    theHuman.mIsReady = true;
    theHuman.mType = 1;
    theHuman.mStartingCitizens = gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    gWorld.GetLobbyOptions().mPlayers[0] = theHuman;
    gResource->GetString(IDS_COMPUTER, theComputerName);
    theComputer.mIsLocal = false;
    theComputer.mName = theComputerName;
    theComputer.mType = 0;
    theComputer.mIsReady = true;
    for (unsigned long i = 1; i < 16; i++) {
        theComputer.mSlot = i;
        theComputer.mColor = i;
        theComputer.mStartingCitizens = gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
        gWorld.GetLobbyOptions().mPlayers[i] = theComputer;
    }
    static_cast<EEServer*>(gCommunications)->StopPing();
    static_cast<EEServer*>(gCommunications)->mLoadState = 1;
    gCommunications->ProcessLocalUserInput(new EEMStartLoading(1, TMGetTime()), kSlotNone);
    gWorld.GetLobbyOptions().GetLock()->WriteUnlock();
}

// FUNCTION: EXE 0x6750a9
// Skips a playing movie on Escape, Return or Space; handles F9 screenshots outside text entry; then the keys
long EEUIManager::UpdateKeyboard()
{
    UWideString theUnused;
    ISEntity* theKey;
    long theResult = kErrorOK;
    bool theStop = false;
    gISKeyboard->GetEntity(kKeyEscape, theKey);
    if (theKey->JustPressed())
        theStop = true;
    gISKeyboard->GetEntity(kKeyReturn, theKey);
    if (theKey->JustPressed())
        theStop = true;
    gISKeyboard->GetEntity(kKeySpace, theKey);
    if (theKey->JustPressed())
        theStop = true;
    if (theStop && gGraphics->mIsMoviePlaying) {
        gGraphics->PostStopMovie();
        while (gGraphics->mIsMoviePlaying)
            PlatSleep(1);
    }
    if (this->GetCurrentForm() != NULL && !this->GetCurrentForm()->GetIsKeyboardCaptured() && !gLoadingSavedGame) {
        gISKeyboard->GetEntity(kKeyF9, theKey);
        if (theKey->JustPressed()) {
            EEPostF9Key();
            return theResult;
        }
    }
    theResult = UIManager::UpdateKeyboard();
    return theResult;
}

// FUNCTION: EXE 0x6751ab
// Skips a playing movie on a mouse click, then handles the mouse
long EEUIManager::UpdateMouse(const GE2DPoint& inMouseLocation)
{
    if ((UIManager::GetLMouseButton()->JustPressed() || UIManager::GetRMouseButton()->JustPressed()) && gGraphics->GetIsMoviePlaying()) {
        gGraphics->PostStopMovie();
        while (gGraphics->mIsMoviePlaying)
            PlatSleep(1);
    }
    return UIManager::UpdateMouse(inMouseLocation);
}

// FUNCTION: EXE 0x67535d
// Appends a callback to a callback vector (std::vector::push_back, ICF-shared)
// (std::vector<long>::push_back (ICF: the callback vectors' push_back))

// FUNCTION: EXE 0x675389
// Copies callback vector elements (std::vector::_Ucopy, ICF-shared)
// (std::vector<long>::_Ucopy (ICF))

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body of EEE2DMarchingIcons::PrepareQueue)
void EEUIManager::FnE123D()
{
}

// ICF-FOLDED: EXE 0x67cf38
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEUIECECampaignNameChanged::ProcessEvent(class UIForm *,class UIEntity *)
{
    return 0;
}

// ICF-FOLDED: EXE 0x67cf38
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEUIECEClose::ProcessEvent(class UIForm *,class UIEntity *)
{
    return 0;
}

