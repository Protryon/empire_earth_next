// C:\EEX\Empire Earth\User Interface Thread\EEUIManager.cpp (file name inferred: no __FILE__; RTTI class
// EEUIManager, vtable 0x44ae90). The game's UI manager: 0x271319..0x2753b1 (EEUIMiniMap.cpp's chains follow). The
// ctor (registers the event factories below and the form-event factory), ??_G, dtor, then the factories in address
// order, each followed by the ??_G bodies of the events it creates first, then the overrides, the message boxes,
// StartScenarioEditor and the callback vectors' COMDATs. The factories have provisional names (by the forms they
// serve); their case ids and classes come from emulating the original dispatch. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureManager.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphics.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBSavedGroup.h"
#include <locale.h>
#include "ee/EECPEngine.h"
#include "ee/EEGamePreferences.h"
#include "ee/EECBUIListBox.h"
#include "ee/EEUIFError.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUIEWonNetStatsEvents.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIBGenericBackground.h"
#include "ee/EEUIManagerEvents.h"
#include "ee/EEControlEventID.h"


class UIFileListBox;
extern bool& gGameStateIsMultiplayer;        // 0x52fa88 (cleared when the scenario editor starts) (provisional name)
extern bool& gLoadingSavedGame;
extern UString gSavedGameExtension;
extern unsigned long& gTexturePixelDepth;
extern FSPath* gDataPath;
void EEPostF9Key();

// the event factories (UIManager's control-event callbacks, then its form-event callback)
UIEvent* EECreateGameEvent(UIForm* inForm, unsigned long inID);
UIEvent* EECreateFormEvent(UIManager* inManager, unsigned long inID);
UIEvent* EECreateWonNetServerSelectEvent(UIForm* inForm, unsigned long inID);

#include "lle/GELine.h"
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
#include "rust/ee_screens.h"   // ee_screens_menus_event_for (ee_screens menus.rs)
// The host's record of one call of ee_screens ui_manager.rs (the manager, StartScenarioEditor's two names,
// UpdateMouse's location)
struct EEUIManagerCall {
    explicit EEUIManagerCall(EEUIManager* inManager) : mManager(inManager), mMouse(NULL) {}
    EEUIManager* mManager;
    UWideString mNames[2];
    const GE2DPoint* mMouse;
};

// Creates the game's UI manager: registers the control and form event factories (the game settings one
// twice), loads the UI database and finds the main game form
EEUIManager::EEUIManager(GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                         const UString& inUnused)
    : UIManager(inRasterizer, gSoundManager, inNumberOfSurfaces, gTexturePixelDepth, gResource, inScreen)
{
    // (the original's one factory per form, registered campaign editor, chat log, civilization builder, game settings,
    // save/load, main game, main screen, post-game summary, scenario intro, scenario load, single player load, game
    // settings again, WON server select, debug sound: one Rust table in that order but WON's, whose ids are its own;
    // the port's controls, the forms, the music and the save form's event: ee_screens ui_manager.rs)
    EEUIManagerCall theCall(this);
    ee_screens_ui_manager_construct(&theCall);
}

// Destroys the UI manager and its scenario editor form
EEUIManager::~EEUIManager()
{
    if (this->mScenarioEditorForm != NULL)
        delete this->mScenarioEditorForm;
    this->mMainGameForm = NULL;
}

// Creates a game form's event for a control id; NULL for an id no factory knows. The original's factories per form
// (EECreateGameSettingsEvent, EECreateSaveLoadEvent, EECreateDebugSoundEvent, EECreateCampaignEditorEvent,
// EECreateChatLogEvent, EECreateCivilizationBuilderEvent, EECreateMainGameEvent, EECreateMainScreenEvent with the
// single player and play campaign factories 0x674200 and 0x673836 folded in, EECreatePostGameSummaryEvent,
// EECreateScenarioIntroEvent, EECreateScenarioLoadEvent, EECreateSinglePlayerLoadEvent) are one Rust table in their
// registration order (ee_screens factories.rs). The save/load screens' kind comes from the screen and the editor;
// their Cancel opens the form the load was made for; the main game form's unit reload is C++.
UIEvent* EECreateGameEvent(UIForm* inForm, unsigned long inID)
{
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    uint32_t theCode = ee_screens_control_event_for(inID, gGameScreenActive, theManager->mInScenarioEditor);
    if (theCode == EE_SCREENS_CPP_CANCEL_LOAD)
        return new EEUIECancel(!gGameScreenActive ? static_cast<UIForm*>(theManager->mScenarioEditorForm)
                                                  : theManager->mMainGameForm);
    if (theCode == EE_SCREENS_CPP_LOAD_UNIT)
        return new EEUIELoadUnit;
    return EECreateScreenEvent(theCode);
}

// Creates the event a form fires when it opens, closes or activates; NULL for other ids (ee_screens factories.rs;
// WON's server select open is C++)
UIEvent* EECreateFormEvent(UIManager* inManager, unsigned long inID)
{
    uint32_t theCode = ee_screens_form_event_for(inID);
    if (theCode == EE_SCREENS_CPP_WNS_OPEN)
        return new EEUIEWNSOpen;
    return EECreateScreenEvent(theCode);
}

// Creates the scenario editor form on first use
void EEUIManager::CreateScenarioEditor()
{
    if (this->mScenarioEditorForm == NULL)
        this->mScenarioEditorForm = new EEUIFScenarioEditor(this);
}

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
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreen());
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

// Creates the main game form as the game's own class; other forms as the base manager does
UIForm* EEUIManager::CreateForm(const UIDbDataForms& inData)
{
    if (inData.mFormID != kFormMainGame)
        return UIManager::CreateForm(inData);
    return new EEUIFMainGame(this, inData);
}

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

// Deletes the scenario editor form
void EEUIManager::DeleteScenarioEditor()
{
    if (this->mScenarioEditorForm != NULL) {
        delete this->mScenarioEditorForm;
        this->mScenarioEditorForm = NULL;
    }
}

// Shows a new message box asking for a text or password
void EEUIManager::DisplayPasswordPrompt(const UWideString& inMessage, unsigned long inAlignment, EEUIFPasswordCallback inCallback,
                                        bool inFlag)
{
    (new EEUIFError(this))->Display(EE_SCREENS_DIALOG_PASSWORD, inMessage, inAlignment, NULL, (EEUIFErrorCallback)inCallback,
                                   NULL, NULL, inFlag);
}

// Shows a new message box with only a Cancel button
void EEUIManager::DisplayCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                EEUIFErrorCallback inCallback, UIForm* inCancel)
{
    (new EEUIFError(this))->Display(EE_SCREENS_DIALOG_CANCEL, inMessage, inAlignment, inForm, inCallback, NULL, inCancel, false);
}

// Shows a new message box with an optional OK button
void EEUIManager::DisplayMessage(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                 EEUIFErrorCallback inCallback, bool inFlag)
{
    (new EEUIFError(this))->Display(EE_SCREENS_DIALOG_MESSAGE, inMessage, inAlignment, inForm, inCallback, NULL, NULL, inFlag);
}

// Shows a new Yes/No message box
void EEUIManager::DisplayQuestion(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inYes,
                                  EEUIFErrorCallback inNo)
{
    (new EEUIFError(this))->Display(EE_SCREENS_DIALOG_QUESTION, inMessage, inAlignment, inForm, inYes, inNo, NULL, false);
}

// Shows a new Yes/No/Cancel message box
void EEUIManager::DisplayYesNoCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                     EEUIFErrorCallback inYes, EEUIFErrorCallback inNo)
{
    (new EEUIFError(this))->Display(EE_SCREENS_DIALOG_YES_NO_CANCEL, inMessage, inAlignment, inForm, inYes, inNo, NULL,
                                   false);
}

// Starts the scenario editor: a local server with a human in slot 0 and computers in slots 1..15, then loading
// (ee_screens ui_manager.rs)
void EEUIManager::StartScenarioEditor()
{
    EEUIManagerCall theCall(this);
    ee_screens_ui_manager_start_editor(&theCall);
}

// Skips a playing movie on Escape, Return or Space; handles F9 screenshots outside text entry; then the keys
// (ee_screens ui_manager.rs)
long EEUIManager::UpdateKeyboard()
{
    EEUIManagerCall theCall(this);
    return (long)ee_screens_ui_manager_update_keyboard(&theCall);
}

// Skips a playing movie on a mouse click, then handles the mouse (ee_screens ui_manager.rs)
long EEUIManager::UpdateMouse(const GE2DPoint& inMouseLocation)
{
    EEUIManagerCall theCall(this);
    theCall.mMouse = &inMouseLocation;
    return (long)ee_screens_ui_manager_update_mouse(&theCall);
}

static_assert(EE_SCREENS_UM_TEXTURE_RESOLUTION_ID == kTextureResolutionControlID &&
              EE_SCREENS_UM_TEXTURE_RESOLUTION_INDEX == kGameSettingsTextureResolutionComboBox &&
              EE_SCREENS_UM_TEXTURE_RESOLUTION_STRING == kTextureResolutionOriginalString &&
              EE_SCREENS_UM_TEXTURE_RESOLUTION_HELP == kTextureResolutionHelpString &&
              EE_SCREENS_UM_FORM_LOBBY == kFormLobby && EE_SCREENS_UM_ENGINE_ID == kLobbyEngineControlID &&
              EE_SCREENS_UM_ENGINE_HEADER == kLobbyEngineHeader && EE_SCREENS_UM_ENGINE_COMBO == kLobbyEngineCombo &&
              EE_SCREENS_UM_ENGINE_LABEL == kLobbyEngineLabel && EE_SCREENS_UM_ENGINE_CLASSIC == kLobbyEngineClassicString &&
              EE_SCREENS_UM_ENGINE_RANDOM == kLobbyEngineRandomString &&
              EE_SCREENS_UM_ENGINE_HEADER_STRING == kLobbyEngineHeaderString &&
              EE_SCREENS_UM_ENGINE_HELP == kLobbyEngineHelpString && kGameSettingsQualityComboBox == 123,
              "ee_screens ui_manager.rs' ids");

// The Rust's calls on the manager, the toolkit, the world and the threads (EE_SCREENS_UM_OP_*), each the original's
extern "C" int64_t ee_screenscpp_ui_manager(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    return EEUIManager::HostOp(*static_cast<EEUIManagerCall*>(inCall), inOp, inA, inB, inC);
}

// (a member: UIManager's UpdateKeyboard and UpdateMouse are protected)
int64_t EEUIManager::HostOp(EEUIManagerCall& ioCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUIManager* theManager = ioCall.mManager;
    EEGameOptions& theLobby = gWorld.GetLobbyOptions();
    static const unsigned long kKeys[] = {kKeyEscape, kKeyReturn, kKeySpace, kKeyF9};
    ISEntity* theKey;
    switch (inOp) {
    case EE_SCREENS_UM_OP_REGISTER:
        theManager->RegisterControlEventCallback(EECreateGameEvent);
        theManager->RegisterControlEventCallback(EECreateWonNetServerSelectEvent);
        theManager->RegisterFormEventCallback(EECreateFormEvent);
        break;
    case EE_SCREENS_UM_OP_LOAD_DATABASE: theManager->LoadDatabase(gDataPath, UString()); break;
    case EE_SCREENS_UM_OP_ADD_LIKE: {
        const EeScreensManagerControl& c = *reinterpret_cast<const EeScreensManagerControl*>(inC);
        return UIControlsTable::AddControlLike(theManager, c.template_id, c.id, c.form_index, c.group, c.left, c.right,
                                               c.top, c.bottom, c.string_begin, c.string_end, c.help);
    }
    case EE_SCREENS_UM_OP_MOVE: {
        const EeScreensManagerMove& m = *reinterpret_cast<const EeScreensManagerMove*>(inC);
        return UIControlsTable::MoveControlsHorizontally(m.form, m.first, m.last, m.left_delta, m.right_delta);
    }
    case EE_SCREENS_UM_OP_ENGINE_COUNT: return EECPEngineChoiceCount();
    case EE_SCREENS_UM_OP_FORMS:
        theManager->mMainGameForm = static_cast<EEUIFMainGame*>(theManager->GetFormByID(kFormMainGame));
        theManager->mInScenarioEditor = false;
        theManager->mScenarioEditorForm = NULL;
        theManager->mB13D = false;
        break;
    case EE_SCREENS_UM_OP_MUSIC:
        theManager->SetMusicVolume(!gGamePreferences.mSettings.mMusicEnabled ? 0.0f : gGamePreferences.mSettings.mMusicVolume);
        break;
    case EE_SCREENS_UM_OP_SAVE_EVENT: {
        theManager->mU138 = 0;
        UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_SL_ACTIVATE);   // (ee_screens save_load.rs)
        theManager->GetFormByID(kFormSaveGame)->SetFormActivateEvent(theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_UM_OP_LOBBY_LOCK: inA != 0 ? theLobby.GetLock()->WriteLock() : theLobby.GetLock()->WriteUnlock(); break;
    case EE_SCREENS_UM_OP_EDITOR_ON:
        theManager->mInScenarioEditor = true;
        gGameStateIsMultiplayer = false;
        break;
    case EE_SCREENS_UM_OP_NEW_SERVER:
        if (gCommunications != NULL)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0;
        break;
    case EE_SCREENS_UM_OP_LOBBY_DEFAULTS:
        theLobby.mMapSize = 1;
        theLobby.mLoadType = 3;
        theLobby.mEpochStart = kTechEpochPrehistoric;
        theLobby.mVariant = 0;
        break;
    case EE_SCREENS_UM_OP_NAME: gResource->GetString(inA == 0 ? IDS_SE_HUMAN : IDS_COMPUTER, ioCall.mNames[inA]); break;
    case EE_SCREENS_UM_OP_CITIZENS: return gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    case EE_SCREENS_UM_OP_SET_PLAYER: {
        const EeScreensManagerPlayer& p = *reinterpret_cast<const EeScreensManagerPlayer*>(inC);
        EEGameOptionsPlayer thePlayer;
        thePlayer.mType = p.kind;
        thePlayer.mName = ioCall.mNames[p.name];
        thePlayer.mSlot = p.slot;
        thePlayer.mColor = p.color;
        thePlayer.mStartingCitizens = p.starting_citizens;
        thePlayer.mIsLocal = p.is_local != 0;
        thePlayer.mIsReady = p.is_ready != 0;
        theLobby.mPlayers[p.slot] = thePlayer;
        break;
    }
    case EE_SCREENS_UM_OP_SERVER_LOAD:
        static_cast<EEServer*>(gCommunications)->StopPing();
        static_cast<EEServer*>(gCommunications)->mLoadState = 1;
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageStartLoading, {1, TMGetTime()}), kSlotNone);
        break;
    case EE_SCREENS_UM_OP_KEY_PRESSED:
        gISKeyboard->GetEntity(kKeys[inA], theKey);
        return theKey->JustPressed();
    case EE_SCREENS_UM_OP_MOVIE_PLAYING: return gGraphics->mIsMoviePlaying;
    case EE_SCREENS_UM_OP_STOP_MOVIE: gGraphics->PostStopMovie(); break;
    case EE_SCREENS_UM_OP_SLEEP: PlatSleep((unsigned long)inA); break;
    case EE_SCREENS_UM_OP_KEYS_FREE:
        return theManager->GetCurrentForm() != NULL && !theManager->GetCurrentForm()->GetIsKeyboardCaptured() &&
               !gLoadingSavedGame;
    case EE_SCREENS_UM_OP_POST_F9: EEPostF9Key(); break;
    case EE_SCREENS_UM_OP_BASE_KEYBOARD: return theManager->UIManager::UpdateKeyboard();
    case EE_SCREENS_UM_OP_BUTTON_PRESSED:
        return (inA == 0 ? UIManager::GetLMouseButton() : UIManager::GetRMouseButton())->JustPressed();
    case EE_SCREENS_UM_OP_BASE_MOUSE: return theManager->UIManager::UpdateMouse(*ioCall.mMouse);
    }
    return 0;
}

void EEUIManager::FnE123D()
{
}
