// C:\EEX\Empire Earth\UI Events\EEScreens.cpp (NEW, no original address: units.EXTRA_UNITS). The host of the game
// screens that are Rust's whole (rust/crates/ee_game/src/screens menus.rs: the main menu, the editors' sub-menu, the single
// player and play campaign screens; settings_form.rs: the game settings; docs/RUST_PLAN.md "R6 game screens", step
// 2; lobby_form.rs: the lobby and its load screens; main_game_options.rs: the in-game options menu; main_game.rs:
// the main game form's events; chat.rs: the chat forms' events; hot_keys.rs: the hot keys; diplomacy.rs; camera.rs:
// the camera, scroll and zoom events; civ_builder.rs: the civilization builder's events; campaign_editor.rs: the
// campaign editor's events): EEScreenEvent's
// ProcessEvent, the C++ events the screens still hang on other forms, and the toolkit's and the game's calls the Rust
// dispatchers make (ee_screenscpp_*; the plain toolkit calls are ee_screens toolkit.rs's, on the toolkit's Rust
// objects). It replaces EEUIEMainScreenEvents.cpp, EEUIEMultiplayerScreenEvents.cpp,
// EEUIEEmpireEarthEditorsEvents.cpp, EEUIESinglePlayerEvents.cpp, EEUIEPlayCampaignEvents.cpp,
// EEUIEGameSettingsEvents.cpp, EEUIELobbyEvents.cpp, EEUIEMainGameOptionsEvents.cpp, EEUIEMainGameEvents.cpp,
// EEUIEChatEvents.cpp, EEUIEHotKeyEvents.cpp, EEUIEDiplomacyEvents.cpp, EEUIECameraEvents.cpp,
// EEUIEScrollZoomEvents.cpp, EEUIEWheelZoom.cpp, EEUIEFormationChange.cpp, EECBUIEvents.cpp and
// EEUIECampaignEditorEvents.cpp.
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
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIESliderChange.h"
#include "lle/NESynchronousServer.h"
#include "lle/UIFileListBox.h"
#include "lle/FSRestorable.h"
#include "lle/UIBFull.h"
#include "lle/UIBitmap.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIFileComboBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECommunications.h"
#include "ee/EEGameState.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEGlobalStorage.h"
#include "ee/EEMouse.h"
#include "ee/EEGameSound.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEDisplayDevices.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECampaignProgress.h"
#include "ee/EEUIBMapBackground.h"
#include "ee/EEStringIDs.h"
#include "ee/EEUICPing.h"
#include "ee/EECPEngine.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEStartingResources.h"
#include "ee/EEGameVariant.h"
#include "ee/EERulesWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUIShell.h"
#include "ee/EEMiniMap.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicObject.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "lle/UGraph.h"
#include "lle/UIGraph.h"
#include "lle/UIStatusBar.h"
#include "lle/UEncryption.h"
#include "ee/EEPGSFormManager.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EECheatCodes.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EERenderObject.h"
#include "ee/EEFormation.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEScenarioEditorIDs.h"
#include "rust/sim/units.h"
#include "ee/EEScreensShims.h"
#include "lle/UITextureBackground.h"

extern float gCameraMaxZoom;   // port: EECameraControl.cpp (the farther zoom-out; 1.0 in the original)
extern GEViewport gU4E8F08;   // 0x4e8f08 (EERenderObject.cpp; the render viewport: the aircraft hot keys' point)
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);   // lle/SEMixer.h
unsigned long EEMenuWidth();   // Empire Earth.cpp
unsigned long EEMenuHeight();

// 0x562fd8 (EEUserInterface.cpp keeps the WON password in it): stored in EEGlobals.cpp (ee/EEGlobalStorage.h),
// constructed here, where its definition was
EE_CONSTRUCT_GLOBAL(UString, gMultiplayerString, (""));
EEUIFMainGame* gSavedMainGameForm;          // 0x563144 (.bss among the UI events' statics; ee/EmpireEarth.h)
static EECivilization sCivilization;        // 0x962f08: the civilization the editors' sub-menu opens the builder on

// (EEScreenEvent::ProcessEvent, EECreateScreenEvent, the event group's ee_screenscpp_event and the grab scroll's
// events: UI Events/EEScreenEvent.cpp)

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

// ---- the lobby's and its load screens' (lobby_form.rs)

static EEGameOptionsPlayer* sLobbyBoot;   // the boot question's slot copy (its answer frees it)
static EEPSaveGameHeader* sLobbyHeader;   // the load screen's header (the next load replaces it)
static FSFileSpec* sLobbyFile;            // its file (the transfers read it; never freed, as the original's)
static EEGameOptions* sLobbyCopy;         // the game type's copy of the options (a random map's)
static UWideString sLobbyMissing;         // the last options check's missing lines, each ended by a line break
static unsigned long sLobbyStartTime;     // 0x562fd0: the lobby's Start time (provisional name)
#define GOPT(inOptions) reinterpret_cast<EEGameOptions*>(inOptions)
#define GOP(inPlayer) reinterpret_cast<EEGameOptionsPlayer*>(inPlayer)
// the saved game checks' (lobby_calls.rs lobby_load; the original EEScreensLobbyLoad's locals): the start check's
// file, the header's options, the lobby options with the load type, the missing lines
static FSFileSpec* sLLCheck;
static EEGameOptions sLLOptions;
static EEGameOptions sLLCurrent;
static std::vector<UWideString> sLLMissing;

// the boot question's answers: the copy the question was asked on, then the lobby's
static void EEScreensBootYes(UIForm* inCopy)
{
    sLobbyBoot = reinterpret_cast<EEGameOptionsPlayer*>(inCopy);
    ee_screens_event(EE_SCREENS_LE_BOOT_YES, NULL, NULL);
}
static void EEScreensBootNo(UIForm* inCopy)
{
    sLobbyBoot = reinterpret_cast<EEGameOptionsPlayer*>(inCopy);
    ee_screens_event(EE_SCREENS_LE_BOOT_NO, NULL, NULL);
}

// an option (EEMGameOptions' numbers; 0 the load type) of the lobby options
static void EEScreensLobbySetOption(uint32_t inOption, long inValue)
{
    EEGameOptions& theOptions = gGameOptions;
    switch (inOption) {
    case 0: theOptions.mLoadType = inValue; break;
    case EE_SCREENS_OPTION_MAP_SIZE: theOptions.mMapSize = inValue; break;
    case EE_SCREENS_OPTION_RESOURCES: theOptions.mResources = inValue; break;
    case EE_SCREENS_OPTION_EPOCH_START: theOptions.mEpochStart = inValue; break;
    case EE_SCREENS_OPTION_EPOCH_END: theOptions.mEpochEnd = inValue; break;
    case EE_SCREENS_OPTION_UNIT_LIMIT: theOptions.mUnitLimit = inValue; break;
    case EE_SCREENS_OPTION_WONDERS: theOptions.mWondersForVictory = inValue; break;
    case EE_SCREENS_OPTION_VARIANT: theOptions.mVariant = inValue; break;
    case EE_SCREENS_OPTION_DIFFICULTY: theOptions.mDifficulty = inValue; break;
    case EE_SCREENS_OPTION_SPEED: theOptions.mGameSpeed = inValue; break;
    case EE_SCREENS_OPTION_RECORD_GAMES: theOptions.mRecordGames = inValue != 0; break;
    case EE_SCREENS_OPTION_CUSTOM_CIVS: theOptions.mAllowCustomCivs = inValue != 0; break;
    case EE_SCREENS_OPTION_REVEAL_MAP: theOptions.mRevealMap = inValue != 0; break;
    case EE_SCREENS_OPTION_LOCK_TEAMS: theOptions.mLockTeams = inValue != 0; break;
    case EE_SCREENS_OPTION_LOCK_SPEED: theOptions.mLockSpeed = inValue != 0; break;
    case EE_SCREENS_OPTION_CHEAT_CODES: theOptions.mCheatCodes = inValue != 0; break;
    case 17: theOptions.mVictory = inValue != 0; break;
    }
}

// a value of the rules' tables the lobby lists (EE_SCREENS_LE_RULE_*)
static uint32_t EEScreensLobbyRule(uint32_t inWhich, uint32_t inArg)
{
    const EEGameOptionRules& theRules = EEGetRules().mGameOptions;   // gameoptions.yaml
    switch (inWhich) {
    case EE_SCREENS_LE_RULE_EPOCHS: return (uint32_t)EEEpochCount();
    case EE_SCREENS_LE_RULE_UNIT_MINIMUM: return (uint32_t)EERuleLong(theRules.mUnitLimitMinimum);
    case EE_SCREENS_LE_RULE_UNIT_MAXIMUM: return (uint32_t)EERuleLong(theRules.mUnitLimitMaximum);
    case EE_SCREENS_LE_RULE_UNIT_STEP: return (uint32_t)theRules.mUnitLimitStep;
    case EE_SCREENS_LE_RULE_WONDERS_MAXIMUM: return (uint32_t)EERuleLong(theRules.mWondersMaximum);
    case EE_SCREENS_LE_RULE_MAP_SIZES: return (uint32_t)theRules.mMapSizes.size();
    case EE_SCREENS_LE_RULE_MAP_SIZE_NAME:
        return inArg < theRules.mMapSizes.size() ? (uint32_t)theRules.mMapSizes[inArg].mNameID : 0;
    case EE_SCREENS_LE_RULE_DIFFICULTIES: return (uint32_t)theRules.mDifficulties.size();
    case EE_SCREENS_LE_RULE_DIFFICULTY_NAME:
        return inArg < theRules.mDifficulties.size() ? (uint32_t)theRules.mDifficulties[inArg].mNameID : 0;
    case EE_SCREENS_LE_RULE_SPEEDS: return (uint32_t)theRules.mSpeeds.size();
    case EE_SCREENS_LE_RULE_SPEED_NAME:
        return inArg < theRules.mSpeeds.size() ? (uint32_t)theRules.mSpeeds[inArg].mNameID : 0;
    case EE_SCREENS_LE_RULE_VARIANTS: return (uint32_t)gGameVariants.size();
    case EE_SCREENS_LE_RULE_VARIANT_SELECTABLE:
        return inArg < gGameVariants.size() && gGameVariants[inArg]->mSelectable == true;
    case EE_SCREENS_LE_RULE_VARIANT_NAME:
        return inArg < gGameVariants.size() ? (uint32_t)EEGetGameVariantAt(inArg)->mNameID : 0;
    case EE_SCREENS_LE_RULE_RESOURCE_LEVELS: return (uint32_t)EEGetNumberOfResourceLevels();
    case EE_SCREENS_LE_RULE_RESOURCE_LEVEL_NAME: return (uint32_t)EEGetResourceLevel((long)inArg)->GetNameID();
    case EE_SCREENS_LE_RULE_STARTING_RESOURCES_NAME:
        return inArg < EEStartingResources::sTable.size() ? (uint32_t)EEStartingResources::Get(inArg)->mNameID : 0;
    case EE_SCREENS_LE_RULE_EPOCH_TITLE: return (uint32_t)EEEpochTitleID((long)(int32_t)inArg);
    case EE_SCREENS_LE_RULE_GENERATORS: return (uint32_t)EERMGetNumberOfGenerators();
    case EE_SCREENS_LE_RULE_ENGINES: return (uint32_t)EECPEngineChoiceCount();
    case EE_SCREENS_LE_RULE_DEFAULT_COLOUR: return inArg < 16 ? (uint32_t)gDefaultPlayerColors[inArg] : 0;
    case EE_SCREENS_LE_RULE_STARTING_CITIZENS: return gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    }
    return 0;
}

// ---- the in-game options menu's (main_game_options.rs): its questions' answers

static void EEScreensResignYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_RESIGN_YES, NULL, NULL); }
static void EEScreensRestartYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_RESTART_YES, NULL, NULL); }
static void EEScreensToEditorYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_RETURN_TO_EDITOR_YES, NULL, NULL); }
static void EEScreensExitYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_EXIT_YES, NULL, NULL); }

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

// ---- the hot keys' (hot_keys.rs): inForm is the main game form

// the last snapshot's complex units (EE_SCREENS_GAME_HK_CENTER's record index), kept by ee_screenscpp_hot_key_keep
static std::vector<EEComplexUnit*> sHotKeyUnits;

// the aircraft's target (hot_keys_ffi.rs): the map point under the game view's centre, or its tile's centre
extern "C" void ee_screenscpp_hot_key_target(float* outXY)
{
    GE3DPoint theTarget;
    GE2DPoint theCursor(gU4E8F08.GetXCenter(), gU4E8F08.GetYCenter());
    if (gWorld.mMap->GetLocationAt(theCursor, theTarget, true) == NULL) {
        long theX;
        long theY;
        gWorld.mMap->GetTileAt(theCursor, theX, theY);
        theTarget.SetX(theX + 0.5f);
        theTarget.SetY(theY + 0.5f);
    }
    outXY[0] = theTarget.GetX();
    outXY[1] = theTarget.GetY();
}

// the snapshot's complex units (hud_views.rs hot_key_units), EE_SCREENS_GAME_HK_CENTER's by the snapshot's index
extern "C" void ee_screenscpp_hot_key_keep(const uint64_t* inUnits, size_t inCount)
{
    sHotKeyUnits.clear();
    for (size_t i = 0; i < inCount; i++)
        sHotKeyUnits.push_back(reinterpret_cast<EEComplexUnit*>((uintptr_t)inUnits[i]));
}

static uint32_t EEScreensHotKeyCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    switch (inOp) {
    case EE_SCREENS_GAME_HK_UPDATE_SELECTION:
        thePlayer->UpdateSelection(NULL, true);
        break;
    case EE_SCREENS_GAME_HK_RESET_SAVED:
        thePlayer->mSavedGroups.Reset(0x14, true);
        break;
    case EE_SCREENS_OP_HK_GROUP_BUTTON:
        EEUIShell::Op(inForm->GetControl(inA), EE_SCREENS_SKOP_SG_SET_UNIT, (int64_t)thePlayer->mSelectedGroup->GetFirstUnit());
        break;
    case EE_SCREENS_GAME_HK_SHIFT: {
        ISEntity* theKey;
        gISKeyboard->GetEntity(kKeyShift, theKey);
        return (uint32_t)theKey->GetValue();
    }
    }
    return 0;
}

// ---- the chat log (chat_log.rs): its C++ entry points are inline (ee/EEUIEChatLogEvents.h); a saved game's reads
// and writes

static UWideString sChatLogString;   // the last read's string and colour (ee_screenscpp_cl_last)
static GEColor sChatLogColour;

extern "C" long ee_screenscpp_cl_read(void* inFile, uint32_t inKind, uint32_t* outValue)
{
    FSFile& theFile = *static_cast<FSFile*>(inFile);
    if (inKind == 0) {
        unsigned long theValue = 0;
        long theResult = theFile.Read(theValue);
        *outValue = (uint32_t)theValue;
        return theResult;
    }
    if (inKind == 1)
        return sChatLogString.Load(theFile);
    return sChatLogColour.Load(theFile);
}

extern "C" size_t ee_screenscpp_cl_last(uint32_t inKind, uint8_t* outText, size_t inCapacity)
{
    if (inKind == 2) {
        const float theColour[3] = {sChatLogColour.mRed, sChatLogColour.mGreen, sChatLogColour.mBlue};
        memcpy(outText, theColour, inCapacity < 12 ? inCapacity : 12);
        return 12;
    }
    size_t theLength = sChatLogString.GetTCHARLength();
    memcpy(outText, (const char*)sChatLogString, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

extern "C" long ee_screenscpp_cl_write(void* inFile, uint32_t inKind, uint32_t inValue, const uint8_t* inBytes,
                                       size_t inLength)
{
    FSFile& theFile = *static_cast<FSFile*>(inFile);
    if (inKind == 0) {
        unsigned long theValue = inValue;
        return theFile.Write(theValue);
    }
    if (inKind == 1)
        return Text(inBytes, inLength).Save(theFile);
    GEColor theColour;
    memcpy(&theColour.mRed, inBytes, 4);
    memcpy(&theColour.mGreen, inBytes + 4, 4);
    memcpy(&theColour.mBlue, inBytes + 8, 4);
    return theColour.Save(theFile);
}

// ---- the chat forms' (chat.rs): inForm is the chat form or the chat log form (NULL for the log's posted close)
static uint32_t EEScreensChatCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    switch (inOp) {
    case EE_SCREENS_OP_CH_MOUSE_INSIDE:
        return inForm->GetRectangle().Contains(inForm->GetMouseLocation());
    case EE_SCREENS_GAME_CL_UNPAUSE:
        gWorld.UnpauseGame();
        break;
    default:
        return EEScreensHotKeyCall(inForm, inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the main game form's events' (main_game.rs): inForm is the main game form (or the editor's, for the media
// dump); the behaviour menu's calls are on the manager's main game form, as the original's
static uint32_t EEScreensMainGameCall(EEUIFMainGame* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    switch (inOp) {
    case EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS:
        inForm->mBuildGate.ClearGhostUnits();
        break;
    case EE_SCREENS_GAME_MG_SET_UI_STATE:
        inForm->SetUIState((long)(int32_t)inA);
        break;
    case EE_SCREENS_GAME_MG_POINTER:
        gMouse->mPointer = (long)(int32_t)inA;
        break;
    case EE_SCREENS_GAME_MG_SHOW_DIALOG:
        return (uint32_t)gPhysics->PostShowDialogMessage();
    case EE_SCREENS_GAME_MG_ENTITY_GROUP:
        inForm->SetEntityGroup((long)inA);
        break;
    case EE_SCREENS_GAME_MG_SET_HIDDEN_MODE:
        gHiddenUnitsMode = (long)inA;
        break;
    case EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED:
        for (EEUnit* theUnit = (EEUnit*)gWorld.mUnits.mFirst; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mTileNode->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex && theComplex->mCanBeHidden)
                theComplex->mHiddenFlags |= kHiddenFlagMoved;
        }
        break;
    case EE_SCREENS_GAME_MG_SET_CAMERA_MODE:
        gWorld.mCameraControl.SetMode((long)inA);
        break;
    case EE_SCREENS_GAME_MG_ZOOM_STYLE:
        gGamePreferences.mSettings.mZoomStyle = gWorld.mCameraControl.mMode;
        break;
    default:
        return EEScreensChatCall(inForm, inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the civilization builder's (civ_builder.rs): inForm is the builder's form or one of its dialogs


// ---- the camera, scroll and zoom events' (camera.rs): inForm is the main game form or the scenario editor's

// the mouse's X axis, Y axis, middle button and wheel (EE_SCREENS_CAM_AXIS_*)
static ISEntity* EEScreensMouseEntity(uint32_t inAxis)
{
    static const unsigned long kAxes[] = {kMouseXAxis, kMouseYAxis, kMouseMiddleButton, kMouseZAxis};
    ISEntity* theEntity = NULL;
    gISMouse->GetEntity(kAxes[inAxis & 3], theEntity);
    return theEntity;
}

static uint32_t EEScreensCameraCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    EECameraControl& theCamera = gWorld.mCameraControl;
    switch (inOp) {
    case EE_SCREENS_GAME_CAM_FREEZE:
        gMouse->mIsFrozen = inA != 0;
        break;
    case EE_SCREENS_GAME_CAM_MOUSE_PRESENT:
        return EEScreensMouseEntity(inA) != NULL;
    case EE_SCREENS_GAME_CAM_MOUSE_VALUE: {
        ISEntity* theEntity = EEScreensMouseEntity(inA);
        return (uint32_t)(inB ? theEntity->GetNormalizedValue() : theEntity->GetValue());
    }
    case EE_SCREENS_GAME_CAM_REVOLVE:
        theCamera.Revolve(GE3DPoint(Float(inA), Float(inB), 0.0f));
        break;
    case EE_SCREENS_GAME_CAM_ROTATE:
        theCamera.Rotate(GE3DPoint(Float(inA), Float(inB), 0.0f));
        break;
    case EE_SCREENS_GAME_CAM_SCROLL:
        theCamera.Scroll(Float(inA), Float(inB));
        break;
    case EE_SCREENS_GAME_CAM_RESET_ZOOM:
        theCamera.ResetZoom();
        break;
    case EE_SCREENS_GAME_CAM_CHANGED:
        theCamera.SetChanged();
        break;
    case EE_SCREENS_GAME_CAM_SET_ZOOM:
        theCamera.SetZoom(Float(inA));
        break;
    case EE_SCREENS_GAME_CAM_COS:
        return Bits(UMCos(Float(inA)));
    default:
        return EEScreensMainGameCall(static_cast<EEUIFMainGame*>(inForm), inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the diplomacy form's (diplomacy.rs): inForm is the diplomacy form

// player inIndex's IsHuman() (a virtual; hud_views.rs diplomacy_player reads the rest)
extern "C" bool ee_screenscpp_player_is_human(uint32_t inIndex)
{
    return gWorld.GetPlayer(inIndex)->IsHuman();
}

// a record message (Actions/EEARust.cpp ee_screenscpp_ga_record) to the local input: the input's result (the sends
// of the screens', the HUD's and the mouse modes' hosts are ee_game hud_sends.rs')
extern "C" int64_t ee_screenscpp_send_local(void* inMessage)
{
    return gCommunications->ProcessLocalUserInput(static_cast<EEMessage*>(inMessage), kSlotNone);
}

static uint32_t EEScreensDiplomacyCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC,
                                       uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_OP_DS_BUTTON_NUMBER:
        return (uint32_t)atoi(static_cast<UIButton*>(inForm->GetControl(inA))->GetGEText()->GetText());
    case EE_SCREENS_OP_DS_LABEL_NUMBER:
        return (uint32_t)static_cast<UITextLabel*>(inForm->GetControl(inA))->GetTextAsInt(0);
    default:
        return EEScreensCameraCall(inForm, inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the post-game summary's (post_game.rs): inForm is the summary's form (25)
static UString sPgsReportFile("B7B226CDB7B226CD2C112D1E2C99B50E9C2355F6C48226BEB7B2086D");   // 0x563108 (encrypted)
static float sPgsRectangle[4];          // the rectangle register (left, top, right, bottom)
static NEProtocol* sPgsProtocol;        // the reports' protocol
static EECampaignFormManager* sPgsCampaign;   // Exit's campaign manager

// the post-game line graph's x values: the game's time (ee_screens post_game_ffi.rs ee_screens_pgs_format)
extern "C" uint32_t ee_screenscpp_game_time() { return (uint32_t)gPhysics->GetGameTime(); }

extern "C" void ee_screenscpp_pgs_history(EeScreensPgsHistory* outHistory)
{
    const EEStatisticsHistory& s = gWorld.mStatistics;
    *outHistory = EeScreensPgsHistory();
    outHistory->records = (uint32_t)s.GetRecords().size();
    outHistory->start_time = s.mStartTime;
    outHistory->end_time = s.mEndTime;
    outHistory->current_record = s.mCurrentRecordIndex;
    outHistory->tile_count = s.mTileCount;
    outHistory->victory_enabled = s.mVictoryEnabled;
    outHistory->is_scenario = s.mIsScenario;
    outHistory->is_campaign = s.mIsCampaign;
    if ((unsigned long)s.mCurrentRecordIndex < s.GetRecords().size())
        outHistory->current_slot = s.GetCurrentRecord().mSlot;
    for (int i = 0; i < 5; i++)
        outHistory->levels[i] = (uint32_t)EEGetResourceLevel(gGameResources)->mAmounts[i];
}

extern "C" void ee_screenscpp_pgs_record(uint32_t inIndex, EeScreensPgsRecord* outRecord)
{
    EEStatisticsRecord& r = gWorld.mStatistics.mRecords[inIndex];
    *outRecord = EeScreensPgsRecord();
    outRecord->colour[0] = r.mColor.mRed;
    outRecord->colour[1] = r.mColor.mGreen;
    outRecord->colour[2] = r.mColor.mBlue;
    outRecord->result = r.mResult;
    outRecord->team = r.mTeam;
    outRecord->end_time = r.mEndTime;
    for (int i = 0; i < 7; i++)
        outRecord->resources[i] = r.mResources[i];
    outRecord->score = r.mScore;
    outRecord->economy_score = r.mEconomyScore;
    outRecord->epoch_score = r.mEpochScore;
    outRecord->military_score = r.mMilitaryScore;
    outRecord->tech_score = r.mTechScore;
    outRecord->slot = r.mSlot;
    outRecord->human = r.mIsHuman;
    for (unsigned long j = 0; j < 31; j++)
        outRecord->values[j] = r.mStatistics.GetValue(j);
    outRecord->territory = r.mStatistics.GetLastValue(0x18);
    outRecord->increases = r.mStatistics.CountIncreases(0x0f);
    EEStatisticsSeries theEpochs = r.mStatistics.GetSeries(12);
    outRecord->epoch_points = (uint32_t)theEpochs.mPoints.size();
    outRecord->epoch_second = theEpochs.mPoints.size() > 1 ? theEpochs.mPoints[1].mValue : 0;
}

static uint32_t EEScreensPostGameCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                      uint32_t inC, uint32_t inD)
{
    auto Ctl = [&](uint32_t inIndex) { return inForm->GetControl(inIndex); };
    auto Bar = [&](uint32_t inIndex) { return static_cast<UIStatusBar*>(inForm->GetControl(inIndex)); };
    switch (inOp) {
    case EE_SCREENS_GAME_PGS_SET_PAGE:
        gGamePreferences.mSettings.mLastOpenPostGameStatsPage = (long)(int32_t)inA;
        break;
    case EE_SCREENS_OP_PGS_SET_R:
        sPgsRectangle[inA & 3] = Float(inB);
        break;
    case EE_SCREENS_OP_PGS_APPLY_R:
        Ctl(inA)->SetRectangle(GERectangle(sPgsRectangle[0], sPgsRectangle[1], sPgsRectangle[2], sPgsRectangle[3]));
        break;
    case EE_SCREENS_GAME_PGS_ADD_SERIES: {
        // the record's statistics series into the line graph's stored graph inB (its graph ops: ee_screens toolkit.rs)
        EEStatisticsRecord& r = gWorld.mStatistics.mRecords[inA];
        const std::vector<EEStatisticsPoint>& thePoints = r.mStatistics.GetSeries(inB).mPoints;
        std::vector<long> thePairs;
        for (unsigned long i = 0; i < thePoints.size(); i++) {
            thePairs.push_back((long)thePoints[i].mTime);
            thePairs.push_back((long)thePoints[i].mValue);
        }
        static_cast<UIGraph*>(Ctl(kPostGameLineGraph))->AddGraphSeries(thePairs.data(), thePoints.size(), inB, r.mColor);
        break;
    }
    case EE_SCREENS_GAME_PGS_NEW_MANAGER:
        inForm->SetFormManager(new EEPGSFormManager);
        break;
    case EE_SCREENS_GAME_PGS_WINDOW: {
        GEWindow* theWindow = inForm->GetManager()->GetRasterizer()->GetWindow(inForm->GetManager()->GetScreen());
        return inA ? theWindow->GetWidth() : theWindow->GetHeight();
    }
    case EE_SCREENS_OP_PGS_IS_BITMAP:
        return dynamic_cast<UIBitmap*>(Ctl(inA)) != NULL;
    case EE_SCREENS_GAME_PGS_PROTOCOL:
        NEProtocol::GetProtocol(0, sPgsProtocol);
        break;
    case EE_SCREENS_GAME_PGS_SCI_DELETE:
        EEUISCIFormManager::Get()->DeleteObjects();
        break;
    case EE_SCREENS_GAME_PGS_SAVE:
        gGamePreferences.Save();
        break;
    default:
        return EEScreensDiplomacyCall(inForm, inOp, inA, inB, inC, inD);   // (the builder's ops: ee_screens civ_whole.rs)
    }
    return 0;
}

// ---- the loading screen's (splash.rs): inForm is the loading screen (form 23)
static uint32_t EEScreensSplashCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                    uint32_t inC, uint32_t inD)
{
    switch (inOp) {
    default:
        return EEScreensPostGameCall(inForm, inEntity, inOp, inA, inB, inC, inD);
    }
    return 0;
}

// ---- the civilization choice form's (civ_choice.rs): the form 35 by the manager, or the event's form
static std::vector<EECivilizationEntry> sPdcsCivs;   // EE_SCREENS_GAME_PDCS_CIVS's, for the names and files
static UWideString sPdcsName;                        // the last EE_SCREENS_TEXT_PDCS_LOAD's civilization
static std::vector<EECivUpgrade> sPdcsUpgrades;

static EECBUIListBox* EEScreensPdcsControl(unsigned long inIndex)
{
    return static_cast<EECBUIListBox*>(gUIManager->GetFormByID(kFormPredefinedCiv)->GetControl(inIndex));
}

static uint32_t EEScreensCivChoiceCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                       uint32_t inC, uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_GAME_PDCS_FILL:
        gCivilizationManager.FillCivilizationList(static_cast<EECBUIListBox*>(inForm->GetControl(inA)));
        break;
    case EE_SCREENS_GAME_PDCS_PAUSE:
        gWorld.PauseGame();
        break;
    case EE_SCREENS_OP_PDCS_CLEAR_STATS:
        EEScreensPdcsControl(4)->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_PDCS_SELECTION: {
        unsigned long theSelection;
        return EEScreensPdcsControl(3)->GetCurrentSelection(theSelection) != kErrorOK ? 0 : theSelection + 1;
    }
    case EE_SCREENS_OP_PDCS_CONTROLS:
        return EEScreensPdcsControl(3) && EEScreensPdcsControl(4) && EEScreensPdcsControl(5);
    case EE_SCREENS_OP_PDCS_SET_UPGRADES:
        EEScreensPdcsControl(4)->SetCivilizationUpgrades(sPdcsUpgrades);
        break;
    case EE_SCREENS_OP_PDCS_HIDE_LABEL:
        EEScreensPdcsControl(5)->SetDisplayMode(kDisplayModeHidden);
        break;
    case EE_SCREENS_OP_PDCS_RESET:
        EEScreensPdcsControl(3)->ResetSelection();
        break;
    default:
        return EEScreensSplashCall(inForm, inEntity, inOp, inA, inB, inC, inD);
    }
    return 0;
}

// ---- the debug sound panel's (debug_sound.rs): the first selected unit's sound of a kind
static EESoundObject* sDebugSound;   // EE_SCREENS_GAME_DSU_SOUND's, for EE_SCREENS_GAME_DSU_VOLUME

static uint32_t EEScreensDebugSoundCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                        uint32_t inC, uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_GAME_DSU_SOUND: {
        EEComplexUnit* theUnit = NULL;
        sDebugSound = NULL;
        if (!EEGetFirstSelectedUnit(theUnit) || theUnit->mComplexUnit == NULL)
            return 0;
        EEUnitClass* theClass = theUnit->mComplexUnit->mUnitClass;
        EEModel* theModel = theClass->mModel;
        switch (inA) {
        case 1: sDebugSound = theClass->mAcknowledgeSounds[0]; break;
        case 2: sDebugSound = theClass->mAcknowledgeSounds[1]; break;
        case 3: sDebugSound = theClass->mCreationSound; break;
        case 4: sDebugSound = theClass->mDeathSound; break;
        case 5: sDebugSound = theClass->mSelectionSounds[0]; break;
        case 6: sDebugSound = theClass->mSelectionSounds[1]; break;
        case 7: sDebugSound = theModel->mSounds1[0]; break;
        case 8: sDebugSound = theModel->mSounds2[0]; break;
        case 9: sDebugSound = theModel->mSounds1[13]; break;
        case 10: sDebugSound = theModel->mSounds2[13]; break;
        }
        return sDebugSound ? 2 : 1;
    }
    case EE_SCREENS_GAME_DSU_VOLUME:
        sDebugSound->SetVolume(Float(inA));
        break;
    default:
        return EEScreensCivChoiceCall(inForm, inEntity, inOp, inA, inB, inC, inD);
    }
    return 0;
}

// ---- the campaign editor's (campaign_editor.rs): inForm is the editor's form (24); its model is Rust's
// (campaign_model.rs: the original's EECampaignEditorBehavior gCampaignEditor, 0x562df0), over the edited campaign
static UWideString sCampaignEditorTranslated;  // the last EE_SCREENS_TEXT_CE_TRANSLATE's

// the questions' answers, on the question's form
#define EE_SCREENS_CE_CALLBACK(Name, Code) \
    static void Name(UIForm* inForm) { ee_screens_event(Code, inForm, NULL); }
EE_SCREENS_CE_CALLBACK(EEScreensCESaveBeforeOpen, EE_SCREENS_CE_SAVE_BEFORE_OPEN)
EE_SCREENS_CE_CALLBACK(EEScreensCEDiscardBeforeOpen, EE_SCREENS_CE_DISCARD_BEFORE_OPEN)
EE_SCREENS_CE_CALLBACK(EEScreensCESaveBeforeNew, EE_SCREENS_CE_SAVE_BEFORE_NEW)
EE_SCREENS_CE_CALLBACK(EEScreensCEDiscardBeforeNew, EE_SCREENS_CE_DISCARD_BEFORE_NEW)
EE_SCREENS_CE_CALLBACK(EEScreensCESaveBeforeMain, EE_SCREENS_CE_SAVE_BEFORE_MAIN)
EE_SCREENS_CE_CALLBACK(EEScreensCEShowMain, EE_SCREENS_CE_SHOW_MAIN)
EE_SCREENS_CE_CALLBACK(EEScreensCESaveBeforeEditor, EE_SCREENS_CE_SAVE_BEFORE_EDITOR)
EE_SCREENS_CE_CALLBACK(EEScreensCEShowEditor, EE_SCREENS_CE_SHOW_EDITOR)
EE_SCREENS_CE_CALLBACK(EEScreensCEDeleteYes, EE_SCREENS_CE_DELETE_YES)
#undef EE_SCREENS_CE_CALLBACK

// The model's host's state (campaign_model_calls.rs, EE_SCREENS_CEM_*): the edited campaign, the strings and the
// files (the original model's calls: Campaign/EECampaignEditorBehavior.cpp)
static EECampaign sCampaignEditorCampaign;              // 0x562df4 (gCampaignEditor's campaign)
static UWideString sCampaignEditorKept;                 // the last string or error
static std::vector<UWideString> sCampaignEditorLines;   // the last scenario list
static std::set<UString> sCampaignEditorFiles;          // Reset's listed files (one set for both lists, as Reset's)
static FSPath* sCampaignEditorPaths[2];                 // Reset's campaign and scenario paths
static FSFileSpec sCampaignEditorDirectory;             // Reset's directory
static FSFileSpec* sCampaignEditorSpec;                 // Reset's found campaign file (FindFile's out)
static FSMemoryFile* sCampaignEditorFile;               // Reset's archived scenario

// (the model's host, the original's ee_screenscpp_ce_campaign and _text, is Rust's over the shims below:
// ee_screens campaign_model_calls.rs; each one call, no logic)
#define CEC sCampaignEditorCampaign
static std::vector<UWideString> sCEMScenarios;   // SET_SCENARIOS' lines (the original's local)
static FSFileSpec sCEMDelete;                    // DELETE's file
static unsigned char sCEMByte;                   // CHECK's byte read
static UWideString sCEMFormat;                   // CHECK's message format
static UWideString sCEMMessage;                  // and the message
static std::vector<UString> sCEMFileList;        // the files' text (Reset's set in order)
extern "C" void ee_screenscpp_cem_kept_clear() { sCampaignEditorKept = UWideString(); }
extern "C" uint32_t ee_screenscpp_cem_string(uint32_t inId)
{ return (uint32_t)gResource->GetString(inId, sCampaignEditorKept); }
extern "C" void ee_screenscpp_cem_set_name(EeScreensShimText inText) { CEC.SetCampaignName(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_author(EeScreensShimText inText) { CEC.SetAuthorName(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_email(EeScreensShimText inText) { CEC.SetAuthorEmail(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_background(EeScreensShimText inText)
{ CEC.SetBackgroundImage(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_civilization(EeScreensShimText inText) { CEC.SetRequiredCiv(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_description(EeScreensShimText inText)
{ CEC.SetCampaignDescription(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_password(EeScreensShimText inText) { CEC.SetPassword(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_scenarios_clear() { sCEMScenarios.clear(); }
extern "C" void ee_screenscpp_cem_scenarios_add(EeScreensShimText inText)
{ sCEMScenarios.push_back(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_scenarios() { CEC.SetCampaignScenarios(sCEMScenarios); }
extern "C" uint32_t ee_screenscpp_cem_load(EeScreensShimText inFile)
{ return (uint32_t)CEC.Load(EEShimText(inFile), sCampaignEditorKept, true); }
extern "C" uint32_t ee_screenscpp_cem_save() { return (uint32_t)CEC.Save(sCampaignEditorKept); }
extern "C" void ee_screenscpp_cem_lines_clear() { sCampaignEditorLines.clear(); }
extern "C" uint32_t ee_screenscpp_cem_list()
{ return (uint32_t)CEC.ListScenarioFiles(sCampaignEditorLines, sCampaignEditorKept, false, false); }
extern "C" void ee_screenscpp_cem_delete_reset() { sCEMDelete = FSFileSpec(); }
extern "C" void ee_screenscpp_cem_delete_volume() { sCEMDelete.SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cem_delete_directory()
{ sCEMDelete.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\"); }
extern "C" void ee_screenscpp_cem_delete_file(EeScreensShimText inText) { sCEMDelete.SetFile(EEShimString(inText)); }
extern "C" bool ee_screenscpp_cem_delete() { return sCEMDelete.Delete() == kErrorOK; }
extern "C" void ee_screenscpp_cem_files_clear() { sCampaignEditorFiles.clear(); }
extern "C" void ee_screenscpp_cem_spec_clear() { sCampaignEditorSpec = NULL; }
extern "C" void ee_screenscpp_cem_file_clear() { sCampaignEditorFile = NULL; }
extern "C" void ee_screenscpp_cem_path_new(uint32_t inIndex) { sCampaignEditorPaths[inIndex] = new FSPath; }
extern "C" void ee_screenscpp_cem_set_directory() { CEC.SetDirectory((const char*)(gStartupDirectory + "data\\")); }
extern "C" void ee_screenscpp_cem_directory_volume() { sCampaignEditorDirectory.SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cem_directory_scenarios()
{ sCampaignEditorDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\"); }
extern "C" void ee_screenscpp_cem_directory_campaigns()
{ sCampaignEditorDirectory.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\"); }
extern "C" void ee_screenscpp_cem_path_add(uint32_t inIndex)
{ sCampaignEditorPaths[inIndex]->AddDirectory(sCampaignEditorDirectory, true); }
extern "C" void ee_screenscpp_cem_list_scenario_files()
{ sCampaignEditorPaths[1]->ListFiles(UString("*") + gScenarioExtension, sCampaignEditorFiles); }
extern "C" void ee_screenscpp_cem_list_campaign_files()
{ sCampaignEditorPaths[0]->ListFiles(UString("*") + gCampaignExtension, sCampaignEditorFiles); }
extern "C" uint32_t ee_screenscpp_cem_built_in(EeScreensShimText inText)
{ return (uint32_t)EECampaignFormManager::GetBuiltInCampaign(EEShimText(inText)); }
extern "C" int64_t ee_screenscpp_cem_find_campaign()
{ return sCampaignEditorPaths[0]->FindFile(UString((const char*)(CEC.GetCampaignName() + (const char*)gCampaignExtension)), sCampaignEditorSpec); }
extern "C" uintptr_t ee_screenscpp_cem_archive_new() { return (uintptr_t)new FSArchive(*sCampaignEditorSpec); }
extern "C" int64_t ee_screenscpp_cem_archive_open(uintptr_t inArchive)
{ return reinterpret_cast<FSArchive*>(inArchive)->Open(); }
extern "C" int64_t ee_screenscpp_cem_archive_find(uintptr_t inArchive, EeScreensShimText inText)
{ return reinterpret_cast<FSArchive*>(inArchive)->FindFile("Scenarios\\" + gU52A280 + EEShimText(inText) + (const char*)gScenarioExtension, sCampaignEditorFile); }
extern "C" void ee_screenscpp_cem_archive_close(uintptr_t inArchive)
{ reinterpret_cast<FSArchive*>(inArchive)->Close(); }
extern "C" void ee_screenscpp_cem_archive_delete(uintptr_t inArchive)
{ delete reinterpret_cast<FSArchive*>(inArchive); }
extern "C" int64_t ee_screenscpp_cem_file_open() { return sCampaignEditorFile->Open(kFileAccessRead); }
extern "C" int64_t ee_screenscpp_cem_file_length(unsigned long* inLength)
{ return sCampaignEditorFile->GetLength(*inLength); }
extern "C" void ee_screenscpp_cem_file_read() { sCampaignEditorFile->Read(sCEMByte); }
extern "C" uint32_t ee_screenscpp_cem_file_byte() { return sCEMByte; }
extern "C" uint64_t ee_screenscpp_cem_file_checksum(EeScreensShimText inText)
{ return (uint64_t)EEGetFileChecksum(sCampaignEditorDirectory, UString((const char*)(EEShimText(inText) + (const char*)gScenarioExtension)), 0); }
extern "C" void ee_screenscpp_cem_differs_format()
{ gResource->GetString(IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS, sCEMFormat); }
extern "C" void ee_screenscpp_cem_differs_print(EeScreensShimText inText)
{ sprintf(sCEMMessage, (const char*)sCEMFormat, (const char*)CEC.GetCampaignName(), (const char*)EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_differs_show() { gUIManager->DisplayMessage(sCEMMessage, true, NULL, NULL, true); }
extern "C" void ee_screenscpp_cem_file_close() { sCampaignEditorFile->Close(); }
extern "C" void ee_screenscpp_cem_file_delete() { delete sCampaignEditorFile; }
extern "C" void ee_screenscpp_cem_path_dereference(uint32_t inIndex) { sCampaignEditorPaths[inIndex]->Dereference(); }
extern "C" void ee_screenscpp_cem_spec_delete() { delete sCampaignEditorSpec; }
extern "C" void ee_screenscpp_cem_name(void* inOut) { EEShimOut(inOut, CEC.GetCampaignName()); }
extern "C" void ee_screenscpp_cem_author(void* inOut) { EEShimOut(inOut, CEC.GetAuthorName()); }
extern "C" void ee_screenscpp_cem_email(void* inOut) { EEShimOut(inOut, CEC.GetAuthorEmail()); }
extern "C" void ee_screenscpp_cem_background(void* inOut) { EEShimOut(inOut, CEC.GetBackgroundImage()); }
extern "C" void ee_screenscpp_cem_civilization(void* inOut) { EEShimOut(inOut, CEC.GetRequiredCiv()); }
extern "C" void ee_screenscpp_cem_description(void* inOut) { EEShimOut(inOut, CEC.GetDescription()); }
extern "C" void ee_screenscpp_cem_password(void* inOut) { EEShimOut(inOut, CEC.GetPassword()); }
extern "C" uint32_t ee_screenscpp_cem_scenario_count() { return (uint32_t)CEC.GetScenarios().size(); }
extern "C" void ee_screenscpp_cem_scenario(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, CEC.GetScenarios()[inIndex]); }
extern "C" void ee_screenscpp_cem_kept(void* inOut) { EEShimOut(inOut, sCampaignEditorKept); }
extern "C" uint32_t ee_screenscpp_cem_line_count() { return (uint32_t)sCampaignEditorLines.size(); }
extern "C" void ee_screenscpp_cem_line(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, sCampaignEditorLines[inIndex]); }
extern "C" void ee_screenscpp_cem_files_snapshot()
{ sCEMFileList.assign(sCampaignEditorFiles.begin(), sCampaignEditorFiles.end()); }
extern "C" uint32_t ee_screenscpp_cem_file_count() { return (uint32_t)sCEMFileList.size(); }
extern "C" void ee_screenscpp_cem_file_name(uint32_t inIndex, void* inOut) { EEShimOut(inOut, sCEMFileList[inIndex]); }
extern "C" void ee_screenscpp_cem_scenario_extension(void* inOut) { EEShimOut(inOut, gScenarioExtension); }
extern "C" void ee_screenscpp_cem_campaign_extension(void* inOut) { EEShimOut(inOut, gCampaignExtension); }

static uint32_t EEScreensCampaignEditorCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA,
                                            uint32_t inB, uint32_t inC, uint32_t inD)
{
    // the control op's control (read only by the ops that take one)
    auto Ctl = [&]() { return inForm->GetControl(inA); };
    auto Combo = [&]() { return static_cast<UIFileComboBox*>(Ctl()); };
    switch (inOp) {
    case EE_SCREENS_OP_CE_IS:
        switch (inB) {
        case EE_SCREENS_CE_IS_EDIT: return dynamic_cast<UIEditBox*>(Ctl()) != NULL;
        case EE_SCREENS_CE_IS_LABEL: return dynamic_cast<UITextLabel*>(Ctl()) != NULL;
        case EE_SCREENS_CE_IS_LIST: return dynamic_cast<UIListBox<UWideString>*>(Ctl()) != NULL;
        default: return dynamic_cast<UIFileComboBox*>(Ctl()) != NULL;
        }
    default:
        return EEScreensDebugSoundCall(inForm, inEntity, inOp, inA, inB, inC, inD);
    }
    return 0;
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
    case EE_SCREENS_OP_RESIZE_MENU:
        gUserInterface->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false);
        break;
    case EE_SCREENS_OP_SRAND_TIME:
        srand(TMGetTime());
        break;
    case EE_SCREENS_OP_RAND:
        return rand();
    case EE_SCREENS_GAME_UI_MUSIC_VOLUME:
        gUIManager->SetMusicVolume(Float(inA));
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
    case EE_SCREENS_GAME_PAUSE:
        gWorld.PauseGame();
        break;
    case EE_SCREENS_GAME_APPLY_PORT:
        EEApplyPortSettings(gGamePreferences.GetSettings());
        break;
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
    // the custom campaign form's (custom_campaign.rs): a list box of form inA
    case EE_SCREENS_GAME_SCREEN_COUNT:
        return inA < gDisplayRasterizers.size() ? (uint32_t)gDisplayRasterizers[inA].mScreens.size() : 0;
    case EE_SCREENS_GAME_EXTERNAL_LOBBY:
        EEStartExternalLobby();
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
    case EE_SCREENS_GAME_FORGET_MAIN_FORM:
        gSavedMainGameForm = NULL;
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
    case EE_SCREENS_GAME_SAVE_SEND:
        EEScreensSendSave(sSaveObject);
        break;
    case EE_SCREENS_GAME_SAVE_POST:
        EEScreensPostSave(sSaveObject);
        break;
    case EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM:
        gUserInterface->OpenForm(gSavedMainGameForm);
        break;
    // the campaign scenario select and scenario intro forms' (scenario_screens.rs)
    case EE_SCREENS_GAME_SI_BLINK_OFF:
        EEUISCIFormManager::Get()->SetBlinking(false);
        break;
    case EE_SCREENS_GAME_SI_OPEN_MAP:
        return EEUISCIFormManager::Get()->OpenMap();
    case EE_SCREENS_GAME_SI_OPEN_SOUND:
        return EEUISCIFormManager::Get()->OpenSound();
    case EE_SCREENS_GAME_SI_GET_SCENARIO:
        return EEUISCIFormManager::Get()->GetScenario(EEScreensScenario());
    case EE_SCREENS_GAME_SI_POST_LOAD:
        gUserInterface->PostLoadSavedGame(EEScreensScenario());
        break;
    case EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM:
        EECampaignFormManager::Get()->UpdateChecksum();
        break;
    // the lobby's and its load screens' (lobby_form.rs)
    case EE_SCREENS_GAME_LE_PLAYER_INFO:
        gCommunications->ProcessGuaranteedLocalUserInput(
            new EEMRecord(kMessagePlayerInformation, {(long)(int32_t)inC, (long)inA, 0.0f, (long)inB}), kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_SEND_OPTION:
        gCommunications->ProcessGuaranteedLocalUserInput(
            new EEMRecord(kMessageGameOptions, {(long)(int32_t)inA, (long)(int32_t)inB, UWideString("")}), kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_SET_OPTION:
        EEScreensLobbySetOption(inA, (long)(int32_t)inB);
        break;
    case EE_SCREENS_OP_LE_BITMAP:
        static_cast<UIBitmap*>(Ctl())->SetBitmap(inB ? gUIManager->GetCheckDownOn() : gUIManager->GetCheckDownOff());
        break;
    case EE_SCREENS_OP_LE_PING:
        static_cast<EEUICPing*>(Ctl())->SetPing(Float(inB));
        break;
    case EE_SCREENS_GAME_LE_DISCONNECT:
        gCommunications->ProcessGuaranteedLocalUserInput(new NEMRecord(kNetMessageClientDisconnectionNotification, {false}), kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_START_LOADING:
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageStartLoading, {(unsigned char)inA, (int32_t)inB}),
                                                         kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS:
        gGamePreferences.SetLastGameOptions(gGameOptions);
        break;
    case EE_SCREENS_GAME_LE_START_PING:
        static_cast<EEServer*>(gCommunications)->StartPing();
        break;
    case EE_SCREENS_GAME_LE_RULE:
        return EEScreensLobbyRule(inA, inB);
    // the in-game options menu's (main_game_options.rs)
    case EE_SCREENS_GAME_GO_REQUEST_PAUSE:
        gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientRequestPause), kSlotNone);
        break;
    case EE_SCREENS_GAME_GO_BRIEFING:
        EEDisplayScenarioIntro();
        break;
    case EE_SCREENS_GAME_GO_EXIT:
        EEExitGame();
        break;
    default:
        return EEScreensCampaignEditorCall(theForm, static_cast<UIEntity*>(inEntity), inOp, inA, inB, inC, inD);
    }
    return 0;
}

// ---- the shims of the moved cases (UI lane round 7: rust/crates/ee_game/src/screens/game_calls.rs, game_cpp.rs):
// each one C++ statement; the logic that calls them is Rust's
static UIForm* F(uintptr_t inForm) { return reinterpret_cast<UIForm*>(inForm); }
static EEWonNetGameInfo sWonInfo;   // the lobby's WON game info between its shims (a local of the original cases)
extern "C" uintptr_t ee_screenscpp_slider_event_new(uintptr_t inForm, uint32_t inLabel)
{ return (uintptr_t)new UIESliderChange(static_cast<UITextLabel*>(F(inForm)->GetControl(inLabel)), 1.0f, 0); }
extern "C" void ee_screenscpp_control_set_range_trigger(uintptr_t inForm, uint32_t inControl, uintptr_t inEvent)
{ F(inForm)->GetControl(inControl)->SetTriggerEvent(kControlTriggerRangeChanged, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_slider_event_add_string(uintptr_t inEvent, EeScreensShimText inText)
{ reinterpret_cast<UIESliderChange*>(inEvent)->AddString(0.0f, EEShimText(inText)); }
extern "C" void ee_screenscpp_event_dereference(uintptr_t inEvent)
{ reinterpret_cast<UIEvent*>(inEvent)->Dereference(); }
extern "C" void ee_screenscpp_set_game_view(float inLeft, float inTop, float inRight, float inBottom)
{ EESetGameViewRectangle(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" uintptr_t ee_screenscpp_sound_object(uint32_t inId) { return (uintptr_t)EESoundObject::Get(inId); }
extern "C" void ee_screenscpp_sound_load_media(uintptr_t inSound)
{ reinterpret_cast<EESoundObject*>(inSound)->LoadMedia(); }
extern "C" void ee_screenscpp_sound_play(uintptr_t inSound)
{ gSoundManager->Play(reinterpret_cast<EESoundObject*>(inSound)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" uintptr_t ee_screenscpp_sound_hot_key_alarm() { return (uintptr_t)EESoundObject::Get(kSoundHotKeyAlarm); }
extern "C" uintptr_t ee_screenscpp_game_window() { return (uintptr_t)gGameWindow; }
extern "C" uintptr_t ee_screenscpp_game_window_os(uintptr_t inWindow)
{ return (uintptr_t)reinterpret_cast<GEWindow*>(inWindow)->GetOSRepresentation(); }
extern "C" uint32_t ee_screenscpp_settings_window_mode()
{ return (uint32_t)gGamePreferences.GetSettings().mWindowMode; }
extern "C" void ee_screenscpp_window_set_fixed_size(uintptr_t inWindow, int32_t inFixed)
{ PlatWindowSetFixedSize(reinterpret_cast<PlatWindow*>(inWindow), inFixed); }
extern "C" void ee_screenscpp_window_set_fullscreen(uintptr_t inWindow, int32_t inFull)
{ PlatWindowSetFullscreen(reinterpret_cast<PlatWindow*>(inWindow), inFull); }
extern "C" int32_t ee_screenscpp_display_logical_size(int32_t* inWidth, int32_t* inHeight)
{ return PlatWindowGetDisplayLogicalSize(NULL, inWidth, inHeight); }
extern "C" void ee_screenscpp_window_set_size(uintptr_t inWindow, int32_t inWidth, int32_t inHeight)
{ PlatWindowSetSize(reinterpret_cast<PlatWindow*>(inWindow), inWidth, inHeight); }
extern "C" void ee_screenscpp_civ_clear_upgrades() { sCivilization.mPurchasedUpgrades.clear(); }
extern "C" void ee_screenscpp_civ_reset_points() { sCivilization.mAvailablePoints = sCivilization.mMaxPoints; }
extern "C" int32_t ee_screenscpp_civ_builder_points()
{ return (int32_t)gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints); }
extern "C" void ee_screenscpp_civ_set_points_and_max(int32_t inPoints) { sCivilization.SetPointsAndMax(inPoints); }
extern "C" void ee_screenscpp_open_civ_builder() { gUserInterface->OpenCivilizationBuilder(&sCivilization, false); }
extern "C" void ee_screenscpp_lobby_options_write_lock() { gWorld.GetLobbyOptions().mLock->WriteLock(); }
extern "C" void ee_screenscpp_options_from_last_game() { gGameOptions = gGamePreferences.GetLastGameOptions(); }
extern "C" uintptr_t ee_screenscpp_screen_event_new(uint32_t inEvent)
{ return (uintptr_t)EECreateScreenEvent(inEvent); }
extern "C" void ee_screenscpp_post_event(uintptr_t inEvent)
{ gUserInterface->PostEvent(reinterpret_cast<UIEvent*>(inEvent), NULL); }
extern "C" uintptr_t ee_screenscpp_server_dynamic() { return (uintptr_t)dynamic_cast<EEServer*>(gCommunications); }
extern "C" bool ee_screenscpp_server_paused(uintptr_t inServer)
{ return reinterpret_cast<EEServer*>(inServer)->GetIsGamePaused(); }
extern "C" void ee_screenscpp_server_pause(uintptr_t inServer) { reinterpret_cast<EEServer*>(inServer)->PauseGame(); }
extern "C" void ee_screenscpp_post_pause_information(bool inPaused) { gUserInterface->PostPauseInformation(inPaused); }
extern "C" void ee_screenscpp_intro_set_flag(uint32_t inWhich, bool inValue)
{ (&EEUISCIFormManager::Get()->mIsInGame)[inWhich] = inValue; }
extern "C" void ee_screenscpp_intro_get_sound(uintptr_t inSpec)
{ EEUISCIFormManager::Get()->GetSound(*reinterpret_cast<FSFileSpec*>(inSpec)); }
extern "C" uintptr_t ee_screenscpp_spec_new() { return (uintptr_t)new FSFileSpec; }
extern "C" void ee_screenscpp_spec_file(uintptr_t inSpec, void* inOut)
{ EEShimOut(inOut, reinterpret_cast<FSFileSpec*>(inSpec)->GetFile()); }
extern "C" void ee_screenscpp_spec_delete(uintptr_t inSpec) { delete reinterpret_cast<FSFileSpec*>(inSpec); }
extern "C" void ee_screenscpp_sound_set_latency(uint32_t inMilliseconds) { gSoundManager->SetLatency(inMilliseconds); }
extern "C" void ee_screenscpp_load_sound(EeScreensShimText inFile, unsigned long* inSample, unsigned long* inId)
{ EELoadSound(EEShimString(inFile), 0x31, *inSample, *inId, 0); }
extern "C" void ee_screenscpp_sound_register_one_shot(uint32_t inSample, uint32_t inId)
{ gSoundManager->RegisterOneShotSound(inSample, inId); }
extern "C" uintptr_t ee_screenscpp_form_background(uintptr_t inForm, uint32_t inId)
{ return (uintptr_t)F(inForm)->GetBackgroundByID(inId); }
extern "C" void ee_screenscpp_intro_get_map(uintptr_t inSpec)
{ EEUISCIFormManager::Get()->GetMap(*reinterpret_cast<FSFileSpec*>(inSpec)); }
extern "C" void ee_screenscpp_background_texture_file(uintptr_t inBackground, void* inOut)
{ EEShimOutC(inOut, static_cast<UITextureBackground*>(reinterpret_cast<UIBackground*>(inBackground))->GetTextureFileName()); }
extern "C" void ee_screenscpp_form_remove_background(uintptr_t inForm, uintptr_t inBackground)
{ F(inForm)->RemoveBackground(reinterpret_cast<UIBackground*>(inBackground)); }
extern "C" bool ee_screenscpp_intro_open_map() { return EEUISCIFormManager::Get()->OpenMap(); }
extern "C" void ee_screenscpp_form_lock_background(uintptr_t inForm) { F(inForm)->LockDrawBackground(); }
extern "C" uintptr_t ee_screenscpp_map_background_new(uint32_t inId, EeScreensShimText inFile, uintptr_t inDescription, uintptr_t inForm, uint32_t inPriority)
{ return (uintptr_t) static_cast<UIBackground*>(new EEUIBMapBackground(inId, EEShimString(inFile), *reinterpret_cast<const GETextureDescription*>(inDescription), GERectangle(), UILayoutUtility(), F(inForm), inPriority, 0, 0, true)); }
extern "C" void ee_screenscpp_background_open(uintptr_t inBackground)
{ reinterpret_cast<UIBackground*>(inBackground)->Open(); }
extern "C" void ee_screenscpp_background_load(uintptr_t inBackground)
{ reinterpret_cast<UIBackground*>(inBackground)->LoadBackground(); }
extern "C" void ee_screenscpp_form_unlock_background(uintptr_t inForm) { F(inForm)->UnlockDrawBackground(); }
extern "C" void ee_screenscpp_options_write_lock() { gGameOptions.mLock->WriteLock(); }
extern "C" void ee_screenscpp_options_write_unlock() { gGameOptions.mLock->WriteUnlock(); }
extern "C" void ee_screenscpp_options_read_lock() { gGameOptions.mLock->ReadLock(); }
extern "C" void ee_screenscpp_options_read_unlock() { gGameOptions.mLock->ReadUnlock(); }
extern "C" void ee_screenscpp_options_set_defaults() { gGameOptions.SetDefaults(); }
extern "C" void ee_screenscpp_options_set_load_type(int32_t inLoadType) { gGameOptions.mLoadType = inLoadType; }
extern "C" void ee_screenscpp_options_campaign_difficulty()
{ gGameOptions.mDifficulty = EECampaignFormManager::Get()->mDifficultyLevel; }
extern "C" void ee_screenscpp_options_campaign_name()
{ gGameOptions.mCampaignName = EECampaignFormManager::Get()->mCampaignName; }
extern "C" void ee_screenscpp_options_campaign_checksum()
{ gGameOptions.mCampaignChecksum = EECampaignFormManager::Get()->mChecksum; }
extern "C" void ee_screenscpp_options_campaign_scenario()
{ gGameOptions.mScenarioName = EECampaignFormManager::Get()->mScenarioName; }
extern "C" void ee_screenscpp_options_campaign_first_map()
{ gGameOptions.mMapName = EECampaignFormManager::Get()->mFirstScenario; }
extern "C" void ee_screenscpp_load_scenario_name_from_intro()
{ gLoadScenarioName = EEScreensScenario().GetFile().GetIndexedSubstring(0, EEScreensScenario().GetFile().GetLength() - 5); }
extern "C" void ee_screenscpp_options_map_type_from_intro()
{ gGameOptions.SetMapType((const char*)EEScreensScenario().GetFile()); }
extern "C" void ee_screenscpp_options_set_multiplayer(bool inMultiplayer)
{ gGameOptions.mIsMultiplayer = inMultiplayer; }
extern "C" void ee_screenscpp_delete_communications() { delete gCommunications; }
extern "C" void ee_screenscpp_new_local_server()
{ gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true); }
extern "C" void ee_screenscpp_server_start_threads() { static_cast<EEServer*>(gCommunications)->StartThreads(); }
extern "C" void ee_screenscpp_server_set_load_responses(bool inValue)
{ static_cast<EEServer*>(gCommunications)->mNumLoadResponses = inValue; }
extern "C" void ee_screenscpp_server_stop_ping() { static_cast<EEServer*>(gCommunications)->StopPing(); }
extern "C" void ee_screenscpp_server_set_load_state(bool inValue)
{ static_cast<EEServer*>(gCommunications)->mLoadState = inValue; }
extern "C" void ee_screenscpp_set_local_player_out(bool inValue) { gLocalPlayerOut = inValue; }
extern "C" void ee_screenscpp_send_scenario_start_loading()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageStartLoading, {1, TMGetTime()}), kSlotNone); }
extern "C" void ee_screenscpp_campaign_background_image(void* inOut)
{ EEShimOut(inOut, EECampaignFormManager::Get()->mCampaign.GetBackgroundImage()); }
extern "C" void ee_screenscpp_scenario_prefix(void* inOut) { EEShimOut(inOut, gU52A280); }
extern "C" int32_t ee_screenscpp_data_find_file(EeScreensShimText inFile, uintptr_t* inSpec)
{ return (int32_t)gDataPath->FindFile(EEShimString(inFile), *reinterpret_cast<FSFileSpec**>(inSpec)); }
extern "C" void ee_screenscpp_campaign_set_difficulty(int32_t inLevel)
{ EECampaignFormManager::Get()->mDifficultyLevel = inLevel; }
extern "C" void ee_screenscpp_set_difficulty_level(int32_t inLevel) { gDifficultyLevel = inLevel; }
extern "C" void ee_screenscpp_set_difficulty_level2(int32_t inLevel) { gDifficultyLevel2 = inLevel; }
extern "C" void ee_screenscpp_options_set_difficulty(int32_t inLevel) { gGameOptions.mDifficulty = inLevel; }
extern "C" void ee_screenscpp_combo_text_colour(uintptr_t inForm, uint32_t inControl, float inRed, float inGreen, float inBlue)
{ static_cast<UIComboBox*>(F(inForm)->GetControl(inControl))->GetTextLabel()->GetGEText()->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_label_text_colour(uintptr_t inForm, uint32_t inControl, float inRed, float inGreen, float inBlue)
{ static_cast<UITextLabel*>(F(inForm)->GetControl(inControl))->GetGEText()->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" uintptr_t ee_screenscpp_remove_player_new(uint8_t inSlot)
{ return (uintptr_t)static_cast<EEAction*>(new EEARemovePlayer(inSlot)); }
extern "C" void ee_screenscpp_post_action(uintptr_t inAction)
{ gUserInterface->PostAction(reinterpret_cast<EEAction*>(inAction)); }
extern "C" void ee_screenscpp_action_dereference(uintptr_t inAction)
{ reinterpret_cast<EEAction*>(inAction)->Dereference(); }
extern "C" uintptr_t ee_screenscpp_won_net() { return (uintptr_t)gWonNet; }
extern "C" void ee_screenscpp_won_clear_game_list() { gWonNet->ClearGameList(); }
extern "C" bool ee_screenscpp_won_rejoin_room(bool inFlag) { return gWonNet->RejoinRoom(inFlag); }
extern "C" void ee_screenscpp_won_leave_room() { gWonNet->LeaveRoom(); }
extern "C" void ee_screenscpp_won_info_reset() { sWonInfo = EEWonNetGameInfo(); }
extern "C" void ee_screenscpp_lobby_start_time_now() { sLobbyStartTime = TMGetTime(); }
extern "C" void ee_screenscpp_server_allow_new_players(bool inAllow)
{ static_cast<EEServer*>(gCommunications)->AllowNewPlayers(inAllow); }
extern "C" void ee_screenscpp_server_set_game_state(uint32_t inState)
{ static_cast<EEServer*>(gCommunications)->SetGameState(inState); }
extern "C" void ee_screenscpp_won_info_load() { gWonNet->GetGameInfo(sWonInfo); }
extern "C" uint32_t ee_screenscpp_won_info_flags() { return (uint32_t)sWonInfo.mFlags; }
extern "C" void ee_screenscpp_won_info_set_flags(uint32_t inFlags) { sWonInfo.mFlags = inFlags; }
extern "C" void ee_screenscpp_won_info_set_players(uint8_t inPlayers) { sWonInfo.mPlayers = inPlayers; }
extern "C" void ee_screenscpp_won_info_set_humans(uint8_t inHumans) { sWonInfo.mHumans = inHumans; }
extern "C" uint8_t ee_screenscpp_won_info_players() { return sWonInfo.mPlayers; }
extern "C" uint8_t ee_screenscpp_won_info_humans() { return sWonInfo.mHumans; }
extern "C" void ee_screenscpp_won_info_store() { gWonNet->SetGameInfo(sWonInfo); }
extern "C" void ee_screenscpp_server_reset_xfer() { static_cast<EEServer*>(gCommunications)->ResetXfer(); }
extern "C" void ee_screenscpp_send_full_player(uint32_t inIndex)
{ gCommunications->ProcessGuaranteedLocalUserInput(EENewFullPlayerInformation(gGameOptions.mPlayers[inIndex]), kSlotNone); }
extern "C" int32_t ee_screenscpp_find_generator(uintptr_t inOptions, unsigned long* inLine)
{ return (int32_t)EERMFindGenerator(GOPT(inOptions)->GetMapType(), *inLine); }
extern "C" uintptr_t ee_screenscpp_lobby_boot() { return (uintptr_t)sLobbyBoot; }
extern "C" void ee_screenscpp_server_refuse_boot(uintptr_t inServer)
{ reinterpret_cast<EEServer*>(inServer)->RefuseClient(*sLobbyBoot); }
extern "C" bool ee_screenscpp_lobby_has_file() { return sLobbyHeader && sLobbyFile; }
extern "C" void ee_screenscpp_server_abort_xfer(uint32_t inId)
{ static_cast<NESynchronousServer*>(gCommunications)->AbortXfer(inId); }
extern "C" void ee_screenscpp_server_xfer_lobby_file(uint8_t inSlot, unsigned long* inId)
{ static_cast<NESynchronousServer*>(gCommunications)->XferFileToClient(*sLobbyFile, sLobbyHeader->mChecksum, static_cast<EEServer*>(gCommunications)->GetSendToSlot(inSlot), *inId, false); }
extern "C" void ee_screenscpp_physics_post(uint32_t inMessage) { gPhysics->mThread->Post(inMessage, NULL, NULL, true); }
extern "C" int32_t ee_screenscpp_post_enter_scenario_editor()
{ return (int32_t)gUserInterface->PostEnterScenarioEditor(gUIManager->GetInScenarioEditor()); }
extern "C" uintptr_t ee_screenscpp_user_pause_new() { return (uintptr_t)static_cast<EEAction*>(new EEAUserPauseGame); }
extern "C" void ee_screenscpp_form_music(uint32_t inForm, void* inOut)
{ EEShimOut(inOut, gUIManager->GetFormByID(inForm)->GetMusicFileName()); }
extern "C" void ee_screenscpp_form_set_music(uint32_t inForm, EeScreensShimText inText)
{ gUIManager->GetFormByID(inForm)->SetMusicFileName(EEShimText(inText)); }
extern "C" void ee_screenscpp_manager_pop() { gUIManager->Pop(); }
extern "C" void ee_screenscpp_manager_swap(uint32_t inForm) { gUIManager->SwapForm(gUIManager->GetFormByID(inForm)); }
extern "C" void ee_screenscpp_save_main_game_form() { gSavedMainGameForm = gUIManager->GetMainGameForm(); }

static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
static FSFileSpec* S(uintptr_t inSpec) { return reinterpret_cast<FSFileSpec*>(inSpec); }
static UWideString sShimMessage;   // a validation's message between its shims (a local of the original case)
extern "C" int32_t ee_screenscpp_pdcs_selection(unsigned long* inSelection)
{ return (int32_t)EEScreensPdcsControl(3)->GetCurrentSelection(*inSelection); }
extern "C" void ee_screenscpp_pdcs_line(uint32_t inSelection, void* inOut)
{ EEShimOut(inOut, EEScreensPdcsControl(3)->GetData(inSelection)); }
extern "C" uint32_t ee_screenscpp_display_rasterizer_count() { return (uint32_t)gDisplayRasterizers.size(); }
extern "C" void ee_screenscpp_display_rasterizer_name(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, gDisplayRasterizers[inIndex].mName); }
extern "C" uint32_t ee_screenscpp_display_screen_count(uint32_t inRasterizer)
{ return (uint32_t)gDisplayRasterizers[inRasterizer].mScreens.size(); }
extern "C" void ee_screenscpp_display_screen_name(uint32_t inRasterizer, uint32_t inScreen, void* inOut)
{ EEShimOut(inOut, gDisplayRasterizers[inRasterizer].mScreens[inScreen].mName); }
extern "C" uint32_t ee_screenscpp_rasterizer_count() { return (uint32_t)GERasterizer::GetRasterizerCount(); }
extern "C" void ee_screenscpp_rasterizer_name(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, GERasterizer::GetRasterizer(inIndex)->GetName()); }
extern "C" uintptr_t ee_screenscpp_form_control(uintptr_t inForm, uint32_t inControl)
{ return (uintptr_t)F(inForm)->GetControl(inControl); }
extern "C" uintptr_t ee_screenscpp_check_box_label(uintptr_t inCheck)
{ return (uintptr_t)static_cast<UIControl*>(static_cast<UICheckBox*>(C(inCheck))->GetTextLabel()); }
extern "C" void ee_screenscpp_label_text(uintptr_t inLabel, void* inOut)
{ EEShimOut(inOut, static_cast<UITextLabel*>(C(inLabel))->GetText(0)); }
extern "C" uintptr_t ee_screenscpp_archive_new() { return (uintptr_t)new FSArchive; }
extern "C" void ee_screenscpp_archive_set(uintptr_t inArchive, uintptr_t inSpec)
{ reinterpret_cast<FSArchive*>(inArchive)->SetArchive(*S(inSpec)); }
extern "C" void ee_screenscpp_data_add_archive(uintptr_t inArchive)
{ gDataPath->AddArchive(*reinterpret_cast<FSArchive*>(inArchive), true); }
extern "C" void ee_screenscpp_set_data_paths() { EESetDataPaths(); }
extern "C" void ee_screenscpp_archive_delete(uintptr_t inArchive) { delete reinterpret_cast<FSArchive*>(inArchive); }
extern "C" void ee_screenscpp_scenario_extension(void* inOut) { EEShimOut(inOut, gScenarioExtension); }
extern "C" uintptr_t ee_screenscpp_header_new() { return (uintptr_t)new EEPSaveGameHeader; }
extern "C" void ee_screenscpp_header_delete(uintptr_t inHeader)
{ delete reinterpret_cast<EEPSaveGameHeader*>(inHeader); }
extern "C" int32_t ee_screenscpp_header_load(uintptr_t inHeader, uintptr_t inSpec)
{ return (int32_t)reinterpret_cast<EEPSaveGameHeader*>(inHeader)->LoadFromFile(*S(inSpec)); }
extern "C" void ee_screenscpp_header_set_major(uintptr_t inHeader)
{ reinterpret_cast<EEPSaveGameHeader*>(inHeader)->SetMajorVersion(kSaveGameMajorVersion); }
extern "C" void ee_screenscpp_header_set_minor(uintptr_t inHeader)
{ reinterpret_cast<EEPSaveGameHeader*>(inHeader)->SetMinorVersion(kSaveGameMinorVersion); }
extern "C" void ee_screenscpp_campaign_update_checksum() { EECampaignFormManager::Get()->UpdateChecksum(); }
extern "C" void ee_screenscpp_intro_media_from_header(uintptr_t inHeader)
{ EEUISCIFormManager::Get()->mMediaStrings = reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mMediaStrings; }
extern "C" void ee_screenscpp_intro_prefix_media() { EEScreensMedia::Prefix(EEUISCIFormManager::Get()->mMediaStrings); }
extern "C" void ee_screenscpp_intro_set_file(uintptr_t inSpec) { EEUISCIFormManager::Get()->SetFile(S(inSpec)); }
extern "C" void ee_screenscpp_parse_cheat(EeScreensShimText inText, long* inCode, long* inParameter)
{ gCheatCodes->ParseCheatCode(EEShimText(inText), *inCode, *inParameter); }
extern "C" void ee_screenscpp_send_cheat(int64_t inCode, int64_t inParameter)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageCheatCode, {(long)inCode, (long)inParameter}), kSlotNone); }
extern "C" uintptr_t ee_screenscpp_button_font(uintptr_t inForm, uint32_t inControl)
{ return (uintptr_t)static_cast<UIButton*>(F(inForm)->GetControl(inControl))->GetGEText()->GetFont(); }
extern "C" void ee_screenscpp_button_set_text(uintptr_t inForm, uint32_t inControl, EeScreensShimText inText, uintptr_t inFont)
{ static_cast<UIButton*>(F(inForm)->GetControl(inControl))->SetText(EEShimText(inText), reinterpret_cast<GEFont*>(inFont)); }
extern "C" void ee_screenscpp_last_auto_save(void* inOut) { EEShimOut(inOut, gGamePreferences.mSettings.mLastAutoSave); }
extern "C" void ee_screenscpp_last_quick_save(void* inOut) { EEShimOut(inOut, gGamePreferences.mSettings.mLastQuickSave); }
extern "C" int32_t ee_screenscpp_open_save_game_file(EeScreensShimText inName, EeScreensShimText inDirectory, uintptr_t* inSpec)
{ return (int32_t)EEOpenSaveGameFile(EEShimString(inName), EEShimString(inDirectory), *reinterpret_cast<FSFileSpec**>(inSpec)); }
extern "C" void ee_screenscpp_spec_set_file(uintptr_t inSpec, EeScreensShimText inFile)
{ S(inSpec)->SetFile(EEShimString(inFile)); }
extern "C" void ee_screenscpp_post_load_saved_game(uintptr_t inSpec) { gUserInterface->PostLoadSavedGame(*S(inSpec)); }
extern "C" void ee_screenscpp_display_message(EeScreensShimText inText, bool inFlag)
{ gUIManager->DisplayMessage(EEShimText(inText), true, NULL, 0, inFlag); }
extern "C" uintptr_t ee_screenscpp_manager_form_control(uint32_t inForm, uint32_t inControl)
{ return (uintptr_t)gUIManager->GetFormByID(inForm)->GetControl(inControl); }
extern "C" void ee_screenscpp_label_delete_array(uintptr_t inLabel)
{ static_cast<UITextLabel*>(C(inLabel))->DeleteArray(); }
extern "C" void ee_screenscpp_label_append_text(uintptr_t inLabel, EeScreensShimText inText)
{ static_cast<UITextLabel*>(C(inLabel))->AppendText(EEShimText(inText)); }
extern "C" void ee_screenscpp_campaign_extension(void* inOut) { EEShimOut(inOut, gCampaignExtension); }
extern "C" void ee_screenscpp_campaign_error_reset() { sCampaignError = UWideString(); }
extern "C" bool ee_screenscpp_campaign_load(EeScreensShimText inFile)
{ return EECampaignFormManager::Get()->mCampaign.Load(EEShimText(inFile), sCampaignError, false); }
extern "C" void ee_screenscpp_send_connection_request(EeScreensShimText inName)
{ gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientConnectionRequest, {false, gBinaryChecksum}, {gGameVersion, EEShimText(inName)}), kSlotNone); }
extern "C" uintptr_t ee_screenscpp_path_new() { return (uintptr_t)new FSPath; }
extern "C" void ee_screenscpp_path_dereference(uintptr_t inPath) { reinterpret_cast<FSPath*>(inPath)->Dereference(); }
extern "C" void ee_screenscpp_spec_set_volume_startup(uintptr_t inSpec) { S(inSpec)->SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_spec_set_directory_startup(uintptr_t inSpec, EeScreensShimText inDirectory)
{ S(inSpec)->SetDirectory(gStartupDirectory + EEShimString(inDirectory)); }
extern "C" void ee_screenscpp_path_add_directory(uintptr_t inPath, uintptr_t inSpec)
{ reinterpret_cast<FSPath*>(inPath)->AddDirectory(*S(inSpec), true); }
extern "C" void ee_screenscpp_file_list_add_extension(uintptr_t inList, EeScreensShimText inExtension)
{ static_cast<UIFileListBox*>(C(inList))->AddValidExtension(EEShimString(inExtension)); }
extern "C" void ee_screenscpp_file_list_set_paths(uintptr_t inList, uintptr_t inPath)
{ static_cast<UIFileListBox*>(C(inList))->SetSearchPaths(reinterpret_cast<FSPath*>(inPath)); }
extern "C" void ee_screenscpp_file_list_create(uintptr_t inList)
{ static_cast<UIFileListBox*>(C(inList))->CreateEntries(1); }
extern "C" bool ee_screenscpp_file_list_find(uintptr_t inList, EeScreensShimText inName, unsigned long* inIndex)
{ return static_cast<UIFileListBox*>(C(inList))->FindString(EEShimText(inName), *inIndex) == true; }
extern "C" void ee_screenscpp_list_select(uintptr_t inList, uint32_t inIndex)
{ static_cast<UIFileListBox*>(C(inList))->ChangeSelectionIndex(inIndex, true, false); }
extern "C" void ee_screenscpp_list_scroll_to(uintptr_t inList, uint32_t inIndex)
{ static_cast<UIFileListBox*>(C(inList))->SetScrollBarCurrentRange(inIndex); }
extern "C" int32_t ee_screenscpp_spec_open_read(uintptr_t inSpec) { return (int32_t)S(inSpec)->Open(kFileAccessRead); }
extern "C" void ee_screenscpp_spec_close(uintptr_t inSpec) { S(inSpec)->Close(); }
extern "C" void ee_screenscpp_shim_message_reset() { sShimMessage = UWideString(); }
extern "C" bool ee_screenscpp_validate_save_game(uintptr_t inSpec)
{ return EEValidateSaveGameFile(*S(inSpec), sShimMessage); }
extern "C" void ee_screenscpp_display_shim_message() { gUIManager->DisplayMessage(sShimMessage, true, NULL, 0, true); }
extern "C" void ee_screenscpp_lobby_options_write_unlock() { gWorld.GetLobbyOptions().GetLock()->WriteUnlock(); }
extern "C" void ee_screenscpp_lobby_options_set_defaults() { gWorld.GetLobbyOptions().SetDefaults(); }
extern "C" void ee_screenscpp_lobby_options_set_load_type(int32_t inLoadType)
{ gWorld.GetLobbyOptions().SetLoadType(inLoadType); }
extern "C" void ee_screenscpp_lobby_options_map_type_from_spec(uintptr_t inSpec)
{ gWorld.GetLobbyOptions().SetMapType((const char*)S(inSpec)->GetFile()); }
extern "C" void ee_screenscpp_load_saved_game_name_from_spec(uintptr_t inSpec)
{ gLoadSavedGameName = S(inSpec)->GetFile().GetIndexedSubstring(0, S(inSpec)->GetFile().GetLength() - 5); }
extern "C" void ee_screenscpp_game_state_single() { gGameState.mGameOptions.SetIsMultiplayer(false); }
extern "C" void ee_screenscpp_set_game_state_load_type(int32_t inLoadType) { gGameStateLoadType = inLoadType; }
extern "C" void ee_screenscpp_load_scenario_name_from_spec(uintptr_t inSpec)
{ gLoadScenarioName = S(inSpec)->GetFile().GetIndexedSubstring(0, S(inSpec)->GetFile().GetLength() - 5); }
extern "C" void ee_screenscpp_lobby_header_delete() { delete sLobbyHeader; }
extern "C" void ee_screenscpp_lobby_header_new() { sLobbyHeader = new EEPSaveGameHeader; }
extern "C" void ee_screenscpp_lobby_file_clear() { sLobbyFile = NULL; }
extern "C" uintptr_t ee_screenscpp_debug_sound_label(uint32_t inId)
{ return (uintptr_t)static_cast<UIControl*>(dynamic_cast<UITextLabel*>(gUIManager->GetFormByID(kFormDebugSound)->GetControlById(inId))); }
extern "C" void ee_screenscpp_label_set_text(uintptr_t inLabel, EeScreensShimText inText)
{ static_cast<UITextLabel*>(C(inLabel))->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_pgs_send(EeScreensShimText inHost, uint32_t inPort, EeScreensShimText inPayload)
{ sPgsProtocol->SendSingle(EEShimString(inHost), (unsigned short)inPort, inPayload.ptr, (unsigned long)inPayload.len); }
extern "C" void ee_screenscpp_pdcs_name_reset() { sPdcsName = UWideString(); }
extern "C" void ee_screenscpp_pdcs_upgrades_clear() { sPdcsUpgrades.clear(); }
extern "C" uint32_t ee_screenscpp_pdcs_load(EeScreensShimText inFile)
{ return (uint32_t)EELoadCivilization(EEShimString(inFile), EEGetCivilizationDirectory(false), sPdcsName, sPdcsUpgrades); }

static NEProtocol* sShimProtocol;   // the protocol between its shims (a local of the original case)
// shims3 (game_calls_more.rs)
extern "C" uintptr_t ee_screenscpp_player_event_new() { return (uintptr_t)static_cast<EEAction*>(new EEAPlayerEvent); }
extern "C" float ee_screenscpp_hot_key_unit_x(uint32_t inIndex)
{ return sHotKeyUnits[inIndex]->mPhysicalModel.GetLocation().GetX(); }
extern "C" float ee_screenscpp_hot_key_unit_y(uint32_t inIndex)
{ return sHotKeyUnits[inIndex]->mPhysicalModel.GetLocation().GetY(); }
extern "C" float ee_screenscpp_hot_key_unit_z(uint32_t inIndex)
{ return sHotKeyUnits[inIndex]->mPhysicalModel.GetLocation().GetZ(); }
extern "C" void ee_screenscpp_camera_set_changed() { gWorld.mCameraControl.SetChanged(); }
extern "C" void ee_screenscpp_camera_set_location(float inX, float inY, float inZ, bool inFlag1, bool inFlag2)
{ gWorld.mCameraControl.SetLocation(GE3DPoint(inX, inY, inZ), inFlag1, inFlag2); }
extern "C" void ee_screenscpp_mini_map_refresh() { gMiniMap->Refresh(); }
extern "C" void ee_screenscpp_set_camera_following(bool inFollowing) { gCameraFollowing = inFollowing; }
extern "C" uintptr_t ee_screenscpp_hot_key_new() { return (uintptr_t)new UIHotKey; }
extern "C" void ee_screenscpp_hot_key_set_key(uintptr_t inKey, uint32_t inCode)
{ reinterpret_cast<UIHotKey*>(inKey)->SetKey(inCode); }
extern "C" void ee_screenscpp_hot_key_set_just_pressed(uintptr_t inKey, uintptr_t inEvent)
{ reinterpret_cast<UIHotKey*>(inKey)->SetJustPressedEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_chat_set_enter_trigger(uintptr_t inForm, uintptr_t inEvent)
{ F(inForm)->GetControl(kChatTextEditBox)->SetTriggerEvent(kControlTriggerEnterPressed, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_form_add_hot_key(uintptr_t inForm, uintptr_t inKey)
{ F(inForm)->AddHotKey(*reinterpret_cast<UIHotKey*>(inKey), 0); }
extern "C" void ee_screenscpp_hot_key_delete(uintptr_t inKey) { delete reinterpret_cast<UIHotKey*>(inKey); }
extern "C" void ee_screenscpp_ghosts_lock_enter() { gWorld.mGhostUnitsLock->Enter(); }
extern "C" void ee_screenscpp_world_clear_ghosts() { gWorld.ClearGhostUnits(); }
extern "C" void ee_screenscpp_ghosts_lock_leave() { gWorld.mGhostUnitsLock->Leave(); }
extern "C" int32_t ee_screenscpp_spec_delete_file(uintptr_t inSpec) { return (int32_t)S(inSpec)->Delete(); }
extern "C" void ee_screenscpp_spec_create(uintptr_t inSpec) { S(inSpec)->Create(); }
extern "C" int32_t ee_screenscpp_spec_fopen_append(uintptr_t inSpec) { return (int32_t)S(inSpec)->FOpen((char*)"a+"); }
extern "C" void ee_screenscpp_add_chat_log(EeScreensShimText inText, int32_t inValue)
{ gUserInterface->AddChatLogEntry(EEShimText(inText), inValue); }
extern "C" void ee_screenscpp_world_dump_media(uintptr_t inSpec, bool inLeaks)
{ gWorld.DumpMedia(*S(inSpec), inLeaks); }
extern "C" void ee_screenscpp_send_enter_shift()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageEnterShiftMode, {gPhysics->mGameTime}), kSlotNone); }
extern "C" void ee_screenscpp_send_exit_shift()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageExitShiftMode, {gPhysics->mGameTime}), kSlotNone); }
extern "C" void ee_screenscpp_protocol_clear() { sShimProtocol = NULL; }
extern "C" void ee_screenscpp_protocol_load() { NEProtocol::GetProtocol(0, sShimProtocol); }
extern "C" void ee_screenscpp_world_move_player_to_editor()
{ gWorld.MovePlayerToSlot(0xfe, gWorld.GetCurrentPlayer()->GetSlot(), 1, sShimProtocol->GetLocalAddress(gNetworkAdapter), 0); }
extern "C" void ee_screenscpp_time_of_day(uintptr_t inTime) { TMGetTimeOfDay(*reinterpret_cast<TMTime*>(inTime)); }
extern "C" void ee_screenscpp_saved_game_extension(void* inOut) { EEShimOut(inOut, gSavedGameExtension); }
extern "C" void ee_screenscpp_settings_set_last_quick_save(EeScreensShimText inName)
{ gGamePreferences.mSettings.mLastQuickSave = EEShimText(inName); }
extern "C" bool ee_screenscpp_preferences_save() { return gGamePreferences.Save(); }
extern "C" void ee_screenscpp_send_save_to_all(EeScreensShimText inName)
{ dynamic_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(new EEMRecord(kMessageLoadOrSaveGame, {0, 1, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 0, 0, EEShimText(inName)}), 15000, 12); }
extern "C" uintptr_t ee_screenscpp_current_player() { return (uintptr_t)gWorld.GetCurrentPlayer(); }
extern "C" uint64_t ee_screenscpp_lead_unit_id(uint64_t inPlayer) { return ee_units_uu_lead_unit_id(inPlayer); }
extern "C" uintptr_t ee_screenscpp_world_unit(uint64_t inId) { return (uintptr_t)gWorld.GetUnit((unsigned long)inId); }
extern "C" void ee_screenscpp_camera_follow(uintptr_t inUnit, bool inFollow)
{ gWorld.mCameraControl.Follow(reinterpret_cast<EEUnit*>(inUnit), inFollow, 1.0f, 0.0f, 0.0f); }
extern "C" uintptr_t ee_screenscpp_main_group_selected_unit()
{ return (uintptr_t)static_cast<EEUIFMGMainGroup*>(gUIManager->GetMainGameForm()->mGroups[0])->GetSelectedUnit(); }
extern "C" bool ee_screenscpp_unit_has_complex(uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit != NULL; }
extern "C" bool ee_screenscpp_unit_has_class(uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass != NULL; }
extern "C" uint32_t ee_screenscpp_unit_class_command(uintptr_t inUnit, uint32_t inIndex)
{ return (uint32_t)reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mCommands[inIndex]; }
extern "C" uintptr_t ee_screenscpp_formation_new(int32_t inKind) { return (uintptr_t)EEFormation::Create(inKind); }
extern "C" void ee_screenscpp_send_change_formation(int32_t inKind)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageChangeFormation, {(long)inKind, true}), kSlotNone); }
extern "C" void ee_screenscpp_main_form_group_disable(uint32_t inGroup)
{ gUIManager->GetMainGameForm()->GetEntityGroup(inGroup)->Disable(); }
extern "C" void ee_screenscpp_main_form_background_visible(uint32_t inId, bool inVisible)
{ gUIManager->GetMainGameForm()->GetBackgroundByID(inId)->SetVisibility(inVisible); }
extern "C" void ee_screenscpp_formation_set_b5c(uintptr_t inFormation)
{ reinterpret_cast<EEFormation*>(inFormation)->mB5C = true; }
extern "C" void ee_screenscpp_selected_group_set_formation(uintptr_t inFormation)
{ gWorld.mPlayers[gWorld.mCurrentPlayer]->mSelectedGroup->SetFormation(reinterpret_cast<EEFormation*>(inFormation)); }
extern "C" void ee_screenscpp_formation_dereference(uintptr_t inFormation)
{ reinterpret_cast<EEFormation*>(inFormation)->Dereference(); }
extern "C" void ee_screenscpp_bitmap_set_corners(uintptr_t inForm, uint32_t inControl, float inTlu, float inTlv, float inTru, float inTrv, float inBru, float inBrv, float inBlu, float inBlv)
{ static_cast<UIBitmap*>(F(inForm)->GetControl(inControl))->GetBitmap()->SetTextureCoordinates(GETextureCoordinate(inTlu, inTlv), GETextureCoordinate(inTru, inTrv), GETextureCoordinate(inBru, inBrv), GETextureCoordinate(inBlu, inBlv)); }
extern "C" void ee_screenscpp_button_text_player_colour(uintptr_t inForm, uint32_t inControl, uint32_t inPlayer)
{ static_cast<UIButton*>(F(inForm)->GetControl(inControl))->GetGEText()->SetFgColor(gWorld.GetPlayer(inPlayer)->GetColor()); }
extern "C" void ee_screenscpp_label_text_player_colour(uintptr_t inForm, uint32_t inControl, uint32_t inPlayer)
{ static_cast<UITextLabel*>(F(inForm)->GetControl(inControl))->GetGEText()->SetFgColor(gWorld.GetPlayer(inPlayer)->GetColor()); }
extern "C" uint32_t ee_screenscpp_control_id(uintptr_t inControl) { return (uint32_t)C(inControl)->GetId(); }
extern "C" uint32_t ee_screenscpp_control_index(uintptr_t inControl) { return (uint32_t)C(inControl)->GetIndex(); }
extern "C" uint32_t ee_screenscpp_control_border(uintptr_t inControl)
{ return (uint32_t)C(inControl)->GetBorderThickness(); }
extern "C" uintptr_t ee_screenscpp_pgs_status_bar_new(uint32_t inId, uintptr_t inForm, uint32_t inIndex, uint32_t inBorder)
{ return (uintptr_t)static_cast<UIControl*>(new UIStatusBar(inId, GERectangle(sPgsRectangle[0], sPgsRectangle[1], sPgsRectangle[2], sPgsRectangle[3]), F(inForm), inIndex, inBorder)); }
extern "C" void ee_screenscpp_status_bar_clear_text(uintptr_t inBar)
{ static_cast<UIStatusBar*>(C(inBar))->SetText(UWideString(), gFontTahoma8); }
extern "C" void ee_screenscpp_form_add_control(uintptr_t inForm, uintptr_t inControl, uint32_t inGroup)
{ F(inForm)->AddControl(C(inControl), inGroup); }
extern "C" uint32_t ee_screenscpp_control_string_id(uintptr_t inForm, uint32_t inControl)
{ return (uint32_t)F(inForm)->GetControl(inControl)->GetStringTableID(); }
extern "C" void ee_screenscpp_control_set_string_id(uintptr_t inForm, uint32_t inControl, uint32_t inId)
{ F(inForm)->GetControl(inControl)->SetStringTableID(inId); }
extern "C" void ee_screenscpp_bar_copy_text_style(uintptr_t inForm, uint32_t inFrom, uint32_t inTo)
{ *static_cast<UIStatusBar*>(F(inForm)->GetControl(inTo))->GetGEText() = *static_cast<UIStatusBar*>(F(inForm)->GetControl(inFrom))->GetGEText(); }
extern "C" uint32_t ee_screenscpp_form_ui_bit_depth(uintptr_t inForm)
{ return (uint32_t)F(inForm)->GetManager()->GetUITextureBitDepth(); }
extern "C" uint32_t ee_screenscpp_form_screen(uintptr_t inForm)
{ return (uint32_t)F(inForm)->GetManager()->GetScreen(); }
extern "C" uintptr_t ee_screenscpp_texture_load(EeScreensShimText inName, uintptr_t inDescription)
{ return (uintptr_t)gTextureManager->LoadTexture(EEShimString(inName), *reinterpret_cast<const GETextureDescription*>(inDescription)); }
extern "C" void ee_screenscpp_texture_alpha_modulate(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageAlphaMode(kStageOpModulate, -1); }
extern "C" void ee_screenscpp_texture_color_modulate(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageColorMode(kStageOpModulate, -1); }
extern "C" void ee_screenscpp_texture_min_filter(uintptr_t inTexture, uint32_t inMode)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageMinFilterMode(inMode, -1); }
extern "C" void ee_screenscpp_texture_mag_filter(uintptr_t inTexture, uint32_t inMode)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageMagFilterMode(inMode, -1); }
extern "C" void ee_screenscpp_texture_mip_point(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageMipFilterMode(kMipFilterPoint, -1); }
extern "C" void ee_screenscpp_texture_mip_bias(uintptr_t inTexture, float inBias)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageMipMapBias(inBias, -1); }
extern "C" void ee_screenscpp_texture_clamp(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageTextureAddressingMode(kAddressingClamp, -1); }
extern "C" uintptr_t ee_screenscpp_bitmap_new() { return (uintptr_t)new GEBitmap; }
extern "C" void ee_screenscpp_bitmap_set_texture(uintptr_t inBitmap, uintptr_t inTexture, uint32_t inScreen)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), inScreen); }
extern "C" void ee_screenscpp_bitmap_stretch_to_control(uintptr_t inBitmap, uintptr_t inForm, uint32_t inControl, float inWidth, float inHeight, uint32_t inScreen)
{ reinterpret_cast<GEBitmap*>(inBitmap)->ComputeStretchedCoordsAndScale(F(inForm)->GetControl(inControl)->GetRectangle(), F(inForm)->GetControl(inControl)->GetRectangle(), GERectangle(0.0f, 0.0f, inWidth, inHeight), inScreen); }
extern "C" uintptr_t ee_screenscpp_ui_bitmap_new(uint32_t inId, uintptr_t inBitmap, uintptr_t inForm, uint32_t inRectangle, uint32_t inIndex)
{ return (uintptr_t)static_cast<UIControl*>(new UIBitmap(inId, reinterpret_cast<GEBitmap*>(inBitmap), F(inForm)->GetControl(inRectangle)->GetRectangle(), F(inForm), inIndex)); }
extern "C" void ee_screenscpp_bitmap_dereference(uintptr_t inBitmap)
{ reinterpret_cast<GEBitmap*>(inBitmap)->Dereference(); }
extern "C" int32_t ee_screenscpp_spec_first_file(uintptr_t inSearch, EeScreensShimText inPattern, uintptr_t inFound)
{ return (int32_t)S(inSearch)->GetFirstFile(EEShimString(inPattern), *S(inFound)); }
extern "C" int32_t ee_screenscpp_spec_next_file(uintptr_t inSearch, uintptr_t inFound)
{ return (int32_t)S(inSearch)->GetNextFile(*S(inFound)); }
extern "C" void ee_screenscpp_form_remove_control(uintptr_t inForm, uintptr_t inControl, uint32_t inFlag)
{ F(inForm)->RemoveControl(C(inControl), inFlag); }
extern "C" void ee_screenscpp_control_delete(uintptr_t inControl) { delete C(inControl); }
extern "C" void ee_screenscpp_pgs_campaign_first_from_civ()
{ sPgsCampaign->SetFirstScenario(sPgsCampaign->mCampaign.GetRequiredCiv()); }
extern "C" void ee_screenscpp_pgs_campaign_description()
{ sPgsCampaign->SetDescription(sPgsCampaign->mCampaign.GetDescription()); }
extern "C" bool ee_screenscpp_pgs_campaign_is_demo()
{ return sPgsCampaign->mCampaignName.EqualsIgnoreCase(gCampaignEEDemo); }
extern "C" bool ee_screenscpp_pgs_campaign_is_learning()
{ return sPgsCampaign->mCampaignName.EqualsIgnoreCase(gCampaignEELearning); }
extern "C" void ee_screenscpp_pgs_campaign_set_all_open(bool inOpen) { sPgsCampaign->mAllScenariosOpen = inOpen; }
extern "C" void ee_screenscpp_pgs_campaign_set_name(EeScreensShimText inName)
{ sPgsCampaign->SetCampaignName(EEShimText(inName)); }
extern "C" void ee_screenscpp_pgs_campaign_set_first(EeScreensShimText inName)
{ sPgsCampaign->SetFirstScenario(EEShimText(inName)); }
extern "C" void ee_screenscpp_pgs_campaign_set_description(EeScreensShimText inText)
{ sPgsCampaign->SetDescription(EEShimText(inText)); }
extern "C" void ee_screenscpp_lobby_options_read_lock() { gWorld.GetLobbyOptions().mLock->ReadLock(); }
extern "C" void ee_screenscpp_lobby_options_read_unlock() { gWorld.GetLobbyOptions().mLock->ReadUnlock(); }
extern "C" float ee_screenscpp_background_left(uintptr_t inBackground)
{ return reinterpret_cast<UIBackground*>(inBackground)->GetRectangle().mLeft; }
extern "C" float ee_screenscpp_background_top(uintptr_t inBackground)
{ return reinterpret_cast<UIBackground*>(inBackground)->GetRectangle().mTop; }
extern "C" float ee_screenscpp_background_right(uintptr_t inBackground)
{ return reinterpret_cast<UIBackground*>(inBackground)->GetRectangle().mRight; }
extern "C" void ee_screenscpp_background_set_rect(uintptr_t inBackground, float inLeft, float inTop, float inRight, float inBottom)
{ reinterpret_cast<UIBackground*>(inBackground)->SetRectangle(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_background_set_visible(uintptr_t inBackground, bool inVisible)
{ reinterpret_cast<UIBackground*>(inBackground)->SetVisibility(inVisible); }
extern "C" void ee_screenscpp_cb_list_set_auto_select(uintptr_t inList, bool inValue)
{ static_cast<EECBUIListBox*>(C(inList))->SetAutoSelect(inValue); }
extern "C" void ee_screenscpp_cb_list_set_show_points(uintptr_t inList, bool inValue)
{ static_cast<EECBUIListBox*>(C(inList))->mShowPoints = inValue; }
extern "C" void ee_screenscpp_cb_list_set_select_upgrades_only(uintptr_t inList, bool inValue)
{ static_cast<EECBUIListBox*>(C(inList))->mSelectUpgradesOnly = inValue; }
extern "C" void ee_screenscpp_cb_list_set_sort(uintptr_t inList, bool inValue)
{ static_cast<EECBUIListBox*>(C(inList))->SetSortLines(inValue); }
extern "C" void ee_screenscpp_cb_list_set_colour(uintptr_t inList, float inRed, float inGreen, float inBlue)
{ static_cast<EECBUIListBox*>(C(inList))->mC13C = GEColor(inRed, inGreen, inBlue); }
extern "C" void ee_screenscpp_pdcs_civs_clear() { sPdcsCivs.clear(); }
extern "C" void ee_screenscpp_pdcs_civs_get() { gCivilizationManager.GetCivilizations(sPdcsCivs); }
extern "C" uint32_t ee_screenscpp_pdcs_civs_count() { return (uint32_t)sPdcsCivs.size(); }
extern "C" uintptr_t ee_screenscpp_civ_request_new()
{ return (uintptr_t)static_cast<NEMessage*>(new EEMRecord(kMessageCivUpgradesRequested, {gWorld.mCurrentPlayer, 0, sPdcsUpgrades, sPdcsName})); }
extern "C" bool ee_screenscpp_comms_multiplayer() { return gCommunications->GetIsMultiplayerSession(); }
extern "C" void ee_screenscpp_send_to_server(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput(reinterpret_cast<NEMessage*>(inMessage), kSlotServer); }
extern "C" void ee_screenscpp_message_execute(uintptr_t inMessage)
{ reinterpret_cast<NEMessage*>(inMessage)->Execute(); }
extern "C" void ee_screenscpp_message_dereference(uintptr_t inMessage)
{ reinterpret_cast<NEMessage*>(inMessage)->Dereference(); }
extern "C" void ee_screenscpp_combo_add_extension(uintptr_t inForm, uint32_t inControl, EeScreensShimText inExtension)
{ static_cast<UIFileComboBox*>(F(inForm)->GetControl(inControl))->AddValidExtension(EEShimString(inExtension)); }
extern "C" void ee_screenscpp_combo_set_paths(uintptr_t inForm, uint32_t inControl, uintptr_t inPath)
{ static_cast<UIFileComboBox*>(F(inForm)->GetControl(inControl))->SetSearchPaths(reinterpret_cast<FSPath*>(inPath)); }
extern "C" void ee_screenscpp_combo_create(uintptr_t inForm, uint32_t inControl)
{ static_cast<UIFileComboBox*>(F(inForm)->GetControl(inControl))->CreateEntries(1); }
extern "C" void ee_screenscpp_combo_insert_line(uintptr_t inForm, uint32_t inControl, EeScreensShimText inText, uint32_t inIndex)
{ static_cast<UIFileComboBox*>(F(inForm)->GetControl(inControl))->InsertLine(EEShimText(inText), inIndex); }
extern "C" void ee_screenscpp_combo_select(uintptr_t inForm, uint32_t inControl, uint32_t inIndex)
{ static_cast<UIFileComboBox*>(F(inForm)->GetControl(inControl))->SetCurrentSelection(inIndex); }
extern "C" bool ee_screenscpp_lobby_copy_exists() { return sLobbyCopy != NULL; }
extern "C" void ee_screenscpp_lobby_copy_new() { sLobbyCopy = new EEGameOptions; }
extern "C" void ee_screenscpp_lobby_copy_from_options() { *sLobbyCopy = gGameOptions; }
extern "C" void ee_screenscpp_lobby_copy_clear_load_type() { sLobbyCopy->mLoadType = 0; }
extern "C" void ee_screenscpp_options_from_lobby_copy() { gGameOptions = *sLobbyCopy; }
// end shims3

// ---- the lobby's sources, slots, options and saved game checks (lobby_calls.rs; each one statement)
extern "C" uintptr_t ee_screenscpp_lobby_header() { return (uintptr_t)sLobbyHeader; }
extern "C" uintptr_t ee_screenscpp_lobby_header_options(uintptr_t inHeader)
{ return (uintptr_t)&reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mGameOptions; }
extern "C" uintptr_t ee_screenscpp_lobby_copy() { return (uintptr_t)sLobbyCopy; }
extern "C" uintptr_t ee_screenscpp_game_options_ptr() { return (uintptr_t)&gGameOptions; }
extern "C" uintptr_t ee_screenscpp_options_player(uintptr_t inOptions, uint32_t inIndex)
{ return (uintptr_t)&GOPT(inOptions)->mPlayers[inIndex]; }
extern "C" void ee_screenscpp_lobby_boot_new() { sLobbyBoot = new EEGameOptionsPlayer; }
extern "C" uint64_t ee_screenscpp_lp_kind(uintptr_t inP) { return (uint64_t)GOP(inP)->mType; }
extern "C" int64_t ee_screenscpp_lp_loaded_type(uintptr_t inP) { return (int64_t)GOP(inP)->mLoadedType; }
extern "C" float ee_screenscpp_lp_ping(uintptr_t inP) { return GOP(inP)->mPing; }
extern "C" uint64_t ee_screenscpp_lp_slot(uintptr_t inP) { return (uint64_t)GOP(inP)->mSlot; }
extern "C" uint64_t ee_screenscpp_lp_color(uintptr_t inP) { return (uint64_t)GOP(inP)->mColor; }
extern "C" uint64_t ee_screenscpp_lp_team(uintptr_t inP) { return (uint64_t)GOP(inP)->mTeam; }
extern "C" uint64_t ee_screenscpp_lp_citizens(uintptr_t inP) { return (uint64_t)GOP(inP)->mStartingCitizens; }
extern "C" uint64_t ee_screenscpp_lp_transfer(uintptr_t inP) { return (uint64_t)GOP(inP)->mTransferID; }
extern "C" bool ee_screenscpp_lp_is_local(uintptr_t inP) { return GOP(inP)->mIsLocal; }
extern "C" bool ee_screenscpp_lp_is_ready(uintptr_t inP) { return GOP(inP)->mIsReady; }
extern "C" bool ee_screenscpp_lp_b36(uintptr_t inP) { return GOP(inP)->mB36; }
extern "C" uint64_t ee_screenscpp_lp_engine(uintptr_t inP) { return (uint64_t)GOP(inP)->mEngine; }
extern "C" void ee_screenscpp_lp_name(uintptr_t inP, void* inOut) { EEShimOut(inOut, GOP(inP)->mName); }
extern "C" void ee_screenscpp_lp_set_kind(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mType = (decltype(GOP(inP)->mType))inV; }
extern "C" void ee_screenscpp_lp_set_loaded_type(uintptr_t inP, int64_t inV)
{ GOP(inP)->mLoadedType = (decltype(GOP(inP)->mLoadedType))inV; }
extern "C" void ee_screenscpp_lp_set_name(uintptr_t inP, EeScreensShimText inName)
{ GOP(inP)->mName = EEShimText(inName); }
extern "C" void ee_screenscpp_lp_set_ping(uintptr_t inP, float inV) { GOP(inP)->mPing = inV; }
extern "C" void ee_screenscpp_lp_set_slot(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mSlot = (decltype(GOP(inP)->mSlot))inV; }
extern "C" void ee_screenscpp_lp_set_color(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mColor = (decltype(GOP(inP)->mColor))inV; }
extern "C" void ee_screenscpp_lp_set_team(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mTeam = (decltype(GOP(inP)->mTeam))inV; }
extern "C" void ee_screenscpp_lp_set_citizens(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mStartingCitizens = (decltype(GOP(inP)->mStartingCitizens))inV; }
extern "C" void ee_screenscpp_lp_set_transfer(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mTransferID = (decltype(GOP(inP)->mTransferID))inV; }
extern "C" void ee_screenscpp_lp_set_is_local(uintptr_t inP, bool inV) { GOP(inP)->mIsLocal = inV; }
extern "C" void ee_screenscpp_lp_set_is_ready(uintptr_t inP, bool inV) { GOP(inP)->mIsReady = inV; }
extern "C" void ee_screenscpp_lp_set_b36(uintptr_t inP, bool inV) { GOP(inP)->mB36 = inV; }
extern "C" void ee_screenscpp_lp_set_engine(uintptr_t inP, uint64_t inV)
{ GOP(inP)->mEngine = (decltype(GOP(inP)->mEngine))inV; }
extern "C" int64_t ee_screenscpp_lo_load_type(uintptr_t inO) { return (int64_t)GOPT(inO)->mLoadType; }
extern "C" int64_t ee_screenscpp_lo_map_size(uintptr_t inO) { return (int64_t)GOPT(inO)->mMapSize; }
extern "C" int64_t ee_screenscpp_lo_resources(uintptr_t inO) { return (int64_t)GOPT(inO)->mResources; }
extern "C" int64_t ee_screenscpp_lo_epoch_start(uintptr_t inO) { return (int64_t)GOPT(inO)->mEpochStart; }
extern "C" int64_t ee_screenscpp_lo_epoch_end(uintptr_t inO) { return (int64_t)GOPT(inO)->mEpochEnd; }
extern "C" int64_t ee_screenscpp_lo_unit_limit(uintptr_t inO) { return (int64_t)GOPT(inO)->mUnitLimit; }
extern "C" int64_t ee_screenscpp_lo_wonders(uintptr_t inO) { return (int64_t)GOPT(inO)->mWondersForVictory; }
extern "C" int64_t ee_screenscpp_lo_variant(uintptr_t inO) { return (int64_t)GOPT(inO)->mVariant; }
extern "C" int64_t ee_screenscpp_lo_difficulty(uintptr_t inO) { return (int64_t)GOPT(inO)->mDifficulty; }
extern "C" int64_t ee_screenscpp_lo_speed(uintptr_t inO) { return (int64_t)GOPT(inO)->mGameSpeed; }
extern "C" bool ee_screenscpp_lo_multiplayer(uintptr_t inO) { return GOPT(inO)->mIsMultiplayer; }
extern "C" bool ee_screenscpp_lo_record_games(uintptr_t inO) { return GOPT(inO)->mRecordGames; }
extern "C" bool ee_screenscpp_lo_custom_civs(uintptr_t inO) { return GOPT(inO)->mAllowCustomCivs; }
extern "C" bool ee_screenscpp_lo_reveal_map(uintptr_t inO) { return GOPT(inO)->mRevealMap; }
extern "C" bool ee_screenscpp_lo_lock_teams(uintptr_t inO) { return GOPT(inO)->mLockTeams; }
extern "C" bool ee_screenscpp_lo_lock_speed(uintptr_t inO) { return GOPT(inO)->mLockSpeed; }
extern "C" bool ee_screenscpp_lo_cheat_codes(uintptr_t inO) { return GOPT(inO)->mCheatCodes; }
extern "C" bool ee_screenscpp_lo_victory(uintptr_t inO) { return GOPT(inO)->mVictory; }
extern "C" void ee_screenscpp_lobby_set_option(uint32_t inOption, int64_t inValue)
{ EEScreensLobbySetOption(inOption, (long)inValue); }
extern "C" void ee_screenscpp_send_full_options()
{ gCommunications->ProcessGuaranteedLocalUserInput(EENewFullGameOptions(gGameOptions), kSlotNone); }
extern "C" void ee_screenscpp_lobby_boot_delete() { delete sLobbyBoot; }
extern "C" void ee_screenscpp_lobby_boot_clear() { sLobbyBoot = NULL; }
extern "C" void ee_screenscpp_world_lobby_single() { gWorld.GetLobbyOptions().mIsMultiplayer = false; }
extern "C" void ee_screenscpp_lobby_player_set_name(uint32_t inSlot, EeScreensShimText inName)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mName = EEShimText(inName); }
extern "C" int64_t ee_screenscpp_ll_open_lobby(EeScreensShimText inFile, EeScreensShimText inDirectory)
{ return EEOpenSaveGameFile(EEShimString(inFile), EEShimString(inDirectory), sLobbyFile); }
extern "C" uintptr_t ee_screenscpp_lobby_file() { return (uintptr_t)sLobbyFile; }
extern "C" void ee_screenscpp_ll_check_clear() { sLLCheck = NULL; }
extern "C" int64_t ee_screenscpp_ll_open_check(EeScreensShimText inFile, EeScreensShimText inDirectory)
{ return EEOpenSaveGameFile(EEShimString(inFile), EEShimString(inDirectory), sLLCheck); }
extern "C" uintptr_t ee_screenscpp_ll_check_file() { return (uintptr_t)sLLCheck; }
extern "C" void ee_screenscpp_ll_options_from_header(uintptr_t inHeader)
{ sLLOptions = reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mGameOptions; }
extern "C" void ee_screenscpp_ll_missing_clear() { sLLMissing.clear(); }
extern "C" bool ee_screenscpp_ll_check_start(uintptr_t inHeader)
{ return EECheckSavedGameOptions(sLLOptions, gGameOptions, true, sLLMissing, reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mNumPlayers - 1); }
extern "C" void ee_screenscpp_ll_current_from_game() { sLLCurrent = gGameOptions; }
extern "C" void ee_screenscpp_ll_current_load_type(uint32_t inLoadType) { sLLCurrent.mLoadType = inLoadType; }
extern "C" bool ee_screenscpp_ll_check_load(uintptr_t inHeader)
{ return EECheckSavedGameOptions(sLLOptions, sLLCurrent, false, sLLMissing, reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mNumPlayers - 1); }
extern "C" void ee_screenscpp_lobby_missing_clear() { sLobbyMissing = ""; }
extern "C" uint32_t ee_screenscpp_ll_missing_count() { return (uint32_t)sLLMissing.size(); }
extern "C" void ee_screenscpp_lobby_missing_add(uint32_t inIndex) { sLobbyMissing += sLLMissing[inIndex] + "\n"; }
extern "C" void ee_screenscpp_le_map_display(uintptr_t inOptions, void* inOut)
{ EEShimOut(inOut, UWideString(EERMGetGeneratorDisplayName(GOPT(inOptions)->GetMapType()))); }

// a text of the game (EE_SCREENS_TEXT_*) into outText (up to inCapacity bytes); its length in bytes
extern "C" size_t ee_screenscpp_text(void* inForm, uint32_t inWhich, uint32_t inArg, uint8_t* outText, size_t inCapacity)
{
    UWideString theText;
    UIForm* theForm = static_cast<UIForm*>(inForm);
    std::string theList;   // the lists' texts, each ended by a 0 byte
    switch (inWhich) {
    // the loading screen's (splash.rs): the decrypted lines, the registered name, a lobby slot's name
    case EE_SCREENS_TEXT_SS_TITLE:
    case EE_SCREENS_TEXT_SS_COPYRIGHT: {
        UString theEncrypted(inWhich == EE_SCREENS_TEXT_SS_TITLE
                                 ? "B7B2265DB7B2265D2C11B5F7AC114576B5A95DCC84F90596C4116DB6D6B115C626B327BEB7B274D5"
                                 : "B7B227FDB7B227FD1CC12D1714F15D2EFC610D9EB419BD94242B2506FCD147BEA49B253E24C9BD96AC19859696B327BEB7B25FB5");
        UString theDecrypted;
        UEDecryptUString(theEncrypted, theDecrypted);
        theText = theDecrypted;
        break;
    }
    // the post-game summary's (post_game.rs)
    case EE_SCREENS_TEXT_PGS_SERVER: {
        // the stats server's host and port from the report file in the install directory: "host\0port"
        UString theReportFile;
        FSFileSpec theFile;
        char theHost[32];
        char thePort[20];
        UEDecryptUString(sPgsReportFile, theReportFile);
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gGamePreferences.GetSettings().mInstalledFromDirectory);
        theFile.SetFile(theReportFile);
        if (theFile.FOpen("r") != kErrorOK || gCommunications == NULL)
            return (size_t)-1;
        fscanf(theFile, "%s%s", theHost, thePort);
        theFile.Close();
        theList.append(theHost).push_back('\0');
        theList.append(thePort);
        break;
    }
    // the civilization choice form's (civ_choice.rs)
    case EE_SCREENS_TEXT_PDCS_CIV_NAME:
        gResource->GetString(sPdcsCivs[inArg].mNameID, theText);
        break;
    case EE_SCREENS_TEXT_PDCS_CIV_FILE:
        theText = sPdcsCivs[inArg].GetFileName();
        break;
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
    // the lobby's (lobby_form.rs)
    case EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY:
        theText = UWideString((const char*)EERMGetGeneratorDisplayName((unsigned long)inArg));
        break;
    case EE_SCREENS_TEXT_LE_GENERATOR_NAME:
        theText = EERMGetGeneratorName(inArg);
        break;
    case EE_SCREENS_TEXT_LE_ENGINE: {
        // an engine choice (ee/EECPEngine.h): the strings Classic and Random, an AI's manifest name
        const EECPAIEntry* theAI = inArg == kEECPChoiceClassic ? NULL : EECPAIByChoice(inArg);
        if (theAI != NULL)
            theText = UWideString(EECPAIText(theAI->name, theAI->name_len).c_str());
        else
            gResource->GetString(kLobbyEngineClassicString + (inArg == kEECPChoiceRandom ? 1 : 0), theText);
        break;
    }
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
    case EE_SCREENS_TEXT_RASTERIZER_SETTING:
        gGamePreferences.GetSettings().mRasterizerName = std::string((const char*)inText, inLength).c_str();
        break;
    case EE_SCREENS_TEXT_AUDIO_DEVICE: {
        EEGameSettings& theSettings = gGamePreferences.GetSettings();
        snprintf(theSettings.mAudioDevice, sizeof(theSettings.mAudioDevice), "%s",
                 std::string((const char*)inText, inLength).c_str());
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN:
        return EECampaignFormManager::GetBuiltInCampaign(theText);   // (its number: the checksum it must have)
    case EE_SCREENS_TEXT_MESSAGE_NONE_FOUND:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensNoCustomCampaigns, true);
        break;
    case EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensCloseCampaign, true);
        break;
    case EE_SCREENS_TEXT_CS_SCENARIO_NAME:
        EECampaignFormManager::Get()->mScenarioName = theText;
        break;
    case EE_SCREENS_TEXT_QUESTION:
        gUIManager->DisplayQuestion(theText, true, theForm, EEScreensRestoreDefaultYes, NULL);
        break;
    case EE_SCREENS_TEXT_MESSAGE:
        gUIManager->DisplayMessage(theText, true, NULL, 0, inArg != 0);
        break;
    case EE_SCREENS_TEXT_MG_CHAT_LOG:
        gUserInterface->AddChatLogEntry(theText, 0);
        break;
    case EE_SCREENS_TEXT_CH_CHAT:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageInGameChat, {theText}), kSlotNone);
        break;
    case EE_SCREENS_TEXT_CL_APPEND:
        static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox))->AppendText(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_NAME:
        EECampaignFormManager::Get()->SetCampaignName(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION:
        EECampaignFormManager::Get()->SetDescription(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_FIRST:
        EECampaignFormManager::Get()->SetFirstScenario(theText);
        break;
    // the load and save screens' (save_load.rs)
    case EE_SCREENS_TEXT_LOAD_SAVED_NAME:
        gGamePreferences.mSettings.SetLoadSavedGameName(Parts(inText, inLength)[0]);
        break;
    case EE_SCREENS_TEXT_LOAD_SCENARIO_NAME:
        gGamePreferences.mSettings.SetLoadScenarioName(Parts(inText, inLength)[0]);
        break;
    case EE_SCREENS_TEXT_QUESTION_DELETE:
        gUIManager->DisplayQuestion(theText, true, reinterpret_cast<UIForm*>(static_cast<UIFileListBox*>(theForm->GetControl(inArg))),
                                    EEScreensDeleteSelectedFile, 0);
        break;
    case EE_SCREENS_TEXT_SAVE_MESSAGE:
        sSaveObject = reinterpret_cast<UIForm*>(static_cast<NEMessage*>(new EEMRecord(
            kMessageLoadOrSaveGame, {0, 0, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 0, 0, theText})));
        break;
    case EE_SCREENS_TEXT_SAVE_ACTION: {
        std::vector<UString> theParts = Parts(inText, inLength);
        sSaveObject = reinterpret_cast<UIForm*>(static_cast<EEAction*>(new EEASaveGame(theParts[0], true, theParts[1])));
        break;
    }
    case EE_SCREENS_TEXT_SAVE_QUESTION:
        gUIManager->DisplayQuestion(theText, true, sSaveObject, inArg ? EEScreensPostSave : EEScreensSendSave, EEScreensDropSave);
        break;
    // the lobby's (lobby_form.rs)
    case EE_SCREENS_TEXT_LE_MAP_TYPE:
        gGameOptions.mMapType = theText;
        break;
    case EE_SCREENS_TEXT_LE_SEND_MAP_TYPE:
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageGameOptions, {2, (long)inArg, theText}), kSlotNone);
        break;
    case EE_SCREENS_TEXT_LE_CHAT:
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageChat, {0, theText}), kSlotNone);
        break;
    case EE_SCREENS_TEXT_LE_BOOT_QUESTION:
        gUIManager->DisplayQuestion(theText, true, reinterpret_cast<UIForm*>(sLobbyBoot), EEScreensBootYes,
                                    EEScreensBootNo);
        break;
    // the in-game options menu's (main_game_options.rs)
    case EE_SCREENS_TEXT_GO_QUESTION: {
        EEUIManager* theManager = theForm ? static_cast<EEUIManager*>(theForm->GetManager()) : gUIManager;
        EEUIFErrorCallback theYes = inArg == EE_SCREENS_GO_RESIGN_YES    ? EEScreensResignYes
                                  : inArg == EE_SCREENS_GO_RESTART_YES   ? EEScreensRestartYes
                                  : inArg == EE_SCREENS_GO_RETURN_TO_EDITOR_YES ? EEScreensToEditorYes
                                                                          : EEScreensExitYes;
        theManager->DisplayQuestion(theText, true, NULL, theYes, 0);
        break;
    }
    // the debug sound panel's (debug_sound.rs)
    // the post-game summary's (post_game.rs)
    case EE_SCREENS_TEXT_PGS_BAR:
        static_cast<UIStatusBar*>(theForm->GetControl(inArg))->SetText(theText, NULL);
        break;
    case EE_SCREENS_TEXT_PGS_CAMPAIGN_LOAD: {
        UWideString theFileName;
        UWideString theError;
        gUIManager->GetFormByID(kFormCampaignScenarioSelect);
        sPgsCampaign = EECampaignFormManager::Get();
        theFileName = "Campaigns\\" + theText + gCampaignExtension;
        return sPgsCampaign->mCampaign.Load(theFileName, theError, false);
    }
    case EE_SCREENS_TEXT_PGS_CAMPAIGN_NAME:
        sPgsCampaign->SetCampaignName(theText);
        break;
    // the civilization choice form's (civ_choice.rs)
    case EE_SCREENS_TEXT_PDCS_LABEL:
        static_cast<UITextLabel*>(static_cast<UIControl*>(EEScreensPdcsControl(5)))->SetText(theText);
        break;
    // the campaign editor's (campaign_editor.rs)
    case EE_SCREENS_TEXT_CE_QUESTION: {
        // (the main screen's and the editor's questions on the form's manager, as the original's)
        static const EEUIFErrorCallback kYes[] = {EEScreensCESaveBeforeOpen, EEScreensCESaveBeforeNew,
                                                  EEScreensCESaveBeforeMain, EEScreensCESaveBeforeEditor,
                                                  EEScreensCEDeleteYes};
        static const EEUIFErrorCallback kNo[] = {EEScreensCEDiscardBeforeOpen, EEScreensCEDiscardBeforeNew,
                                                 EEScreensCEShowMain, EEScreensCEShowEditor, NULL};
        EEUIManager* theManager = inArg == EE_SCREENS_CE_ASK_MAIN || inArg == EE_SCREENS_CE_ASK_EDITOR
                                      ? static_cast<EEUIManager*>(theForm->GetManager())
                                      : gUIManager;
        theManager->DisplayQuestion(theText, true, theForm, kYes[inArg % 5], kNo[inArg % 5]);
        break;
    }
    case EE_SCREENS_TEXT_CE_APPEND: {
        // the texts, each ended by a 0 byte
        std::vector<UWideString> theLines;
        for (size_t i = 0, theStart = 0; i < inLength; i++)
            if (inText[i] == 0) {
                theLines.push_back(Text(inText + theStart, i - theStart));
                theStart = i + 1;
            }
        static_cast<UIListBox<UWideString>*>(theForm->GetControl(inArg))->AppendStringArray(theLines);
        break;
    }
    case EE_SCREENS_TEXT_CE_BG_EQUALS:
        return static_cast<UIFileComboBox*>(theForm->GetControl(kCampaignEditorBackgroundImageComboBox))
            ->GetListBox()->GetData(inArg).EqualsIgnoreCase(theText);
    case EE_SCREENS_TEXT_CE_TRANSLATE:
        EETranslateString(theText, sCampaignEditorTranslated);
        break;
    }
    return 0;
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

// ---- the lobby's records (lobby_form.rs)

// ---- the screens' read view (ee_screens game_view.rs): a group's values (EE_SCREENS_VIEW_*) or texts
// (EE_SCREENS_VIEW_TEXTS_*) at the moment of the read; every pointer checked (the cases they replace were not)

// the player of a view's argument (an index, or the current player); NULL for none
static EEPlayer* EEScreensViewPlayer(uint32_t inArg)
{
    unsigned long theIndex = inArg == EE_SCREENS_VIEW_CURRENT ? (unsigned long)gWorld.mCurrentPlayer : inArg;
    return theIndex < 17 ? gWorld.GetPlayer(theIndex) : NULL;
}

// (the read views' fills, the original's ee_screenscpp_game_view and _game_texts, are Rust's over the shims below:
// ee_screens game_view_calls.rs; each one read, no logic)
extern "C" uintptr_t ee_screenscpp_gv_sound_manager() { return (uintptr_t)gSoundManager; }
extern "C" bool ee_screenscpp_gv_has_conductor() { return gConductor != NULL; }
extern "C" uintptr_t ee_screenscpp_gv_conductor() { return (uintptr_t)&gConductor->GetConductor(); }
extern "C" uint32_t ee_screenscpp_gv_rasterizer_count() { return (uint32_t)GERasterizer::GetRasterizerCount(); }
extern "C" bool ee_screenscpp_gv_rasterizer_is_current(uint32_t inIndex)
{ return GERasterizer::GetRasterizer(inIndex) == gRasterizer; }
extern "C" uint32_t ee_screenscpp_gv_display_rasterizers() { return (uint32_t)gDisplayRasterizers.size(); }
extern "C" bool ee_screenscpp_gv_not_korean() { return gGamePreferences.IsNotKorean(); }
extern "C" int32_t ee_screenscpp_gv_pgs_page()
{ return (int32_t)gGamePreferences.GetSettings().mLastOpenPostGameStatsPage; }
extern "C" bool ee_screenscpp_gv_pgs_screenshots() { return gGamePreferences.GetSettings().mTakeEndGameScreenshots; }
extern "C" float ee_screenscpp_gv_grab_scroll_speed() { return gGamePreferences.mSettings.GetGrabScrollSpeed(); }
extern "C" bool ee_screenscpp_gv_settings_zoom() { return gGamePreferences.mSettings.mZoomEnabled; }
extern "C" bool ee_screenscpp_gv_zoom_enabled() { return gZoomEnabled; }
extern "C" int32_t ee_screenscpp_gv_difficulty() { return (int32_t)gDifficultyLevel; }
extern "C" bool ee_screenscpp_gv_free_camera() { return gFreeCamera; }
extern "C" bool ee_screenscpp_gv_screen_active() { return gGameScreenActive; }
extern "C" bool ee_screenscpp_gv_saved_main_form() { return gSavedMainGameForm != NULL; }
extern "C" float ee_screenscpp_gv_max_zoom() { return gCameraMaxZoom; }
extern "C" int32_t ee_screenscpp_gv_hidden_mode() { return (int32_t)gHiddenUnitsMode; }
extern "C" bool ee_screenscpp_gv_has_header() { return sLobbyHeader != NULL; }
extern "C" uint32_t ee_screenscpp_gv_header_players() { return (uint32_t)sLobbyHeader->mNumPlayers; }
extern "C" bool ee_screenscpp_gv_comms() { return gCommunications != NULL; }
extern "C" bool ee_screenscpp_gv_is_server() { return gCommunications->IsServer() == true; }
extern "C" bool ee_screenscpp_gv_is_client() { return gCommunications->IsClient() == true; }
extern "C" bool ee_screenscpp_gv_multiplayer_session() { return gCommunications->GetIsMultiplayerSession(); }
extern "C" uint32_t ee_screenscpp_gv_cts_processed()
{ return (uint32_t)gCommunications->GetNumberOfCTsProcessedInPhysics(); }
extern "C" bool ee_screenscpp_gv_server_migrating()
{ return static_cast<NESynchronousServer*>(gCommunications)->IsMigrating(); }
extern "C" bool ee_screenscpp_gv_client_playback()
{ return static_cast<NESynchronousClient*>(gCommunications)->IsPlaybackRecordingSession(); }
extern "C" uint32_t ee_screenscpp_gv_humans() { return (uint32_t)gCommunications->GetNumHumanPlayers(); }
extern "C" int32_t ee_screenscpp_gv_camera_mode() { return (int32_t)gWorld.mCameraControl.mMode; }
extern "C" bool ee_screenscpp_gv_lock_speed() { return gWorld.mGameOptions.GetLockSpeed() != false; }
extern "C" float ee_screenscpp_gv_zoom() { return gWorld.mCameraControl.GetZoom(); }
extern "C" bool ee_screenscpp_gv_scenario_game() { return gWorld.GetGameOptions().GetIsScenarioGame() == true; }
extern "C" float ee_screenscpp_gv_speed_step() { return gWorldTable.GetFloatValue(kWorldGameSpeedStep); }
extern "C" float ee_screenscpp_gv_speed_low() { return gWorldTable.GetFloatValue(kWorldGameSpeedRange); }
extern "C" float ee_screenscpp_gv_speed_high() { return gWorldTable.GetFloatValue2(kWorldGameSpeedRange); }
extern "C" bool ee_screenscpp_gv_main_form() { return gUIManager->GetMainGameForm() != NULL; }
extern "C" uint32_t ee_screenscpp_gv_hot_key_mode() { return (uint32_t)gUIManager->GetMainGameForm()->mHotKeyMode; }
extern "C" float ee_screenscpp_gv_red(uint32_t inPlayer) { return EEGetPlayerColor(inPlayer).mRed; }
extern "C" float ee_screenscpp_gv_green(uint32_t inPlayer) { return EEGetPlayerColor(inPlayer).mGreen; }
extern "C" float ee_screenscpp_gv_blue(uint32_t inPlayer) { return EEGetPlayerColor(inPlayer).mBlue; }
extern "C" uint32_t ee_screenscpp_gv_campaign_checksum() { return (uint32_t)EECampaignFormManager::Get()->mChecksum; }
extern "C" bool ee_screenscpp_gv_all_scenarios_open() { return EECampaignFormManager::Get()->mAllScenariosOpen; }
extern "C" bool ee_screenscpp_gv_intro_in_game() { return EEUISCIFormManager::Get()->mIsInGame; }
extern "C" bool ee_screenscpp_gv_intro_loading() { return EEUISCIFormManager::Get()->mIsLoadingFromGame; }
extern "C" bool ee_screenscpp_gv_intro_campaign() { return EEUISCIFormManager::Get()->mIsCampaign; }
extern "C" bool ee_screenscpp_gv_intro_scroll() { return EEUISCIFormManager::Get()->mScrollBriefingToEnd; }
extern "C" void ee_screenscpp_gt_rasterizer_name(void* inOut)
{ EEShimOut(inOut, gGamePreferences.GetSettings().mRasterizerName); }
extern "C" void ee_screenscpp_gt_default_rasterizer_name(void* inOut)
{ EEShimOut(inOut, EEGamePreferences::sDefaultSettings.mRasterizerName); }
extern "C" void ee_screenscpp_gt_audio_device(void* inOut)
{ EEShimOut(inOut, UWideString(gGamePreferences.GetSettings().mAudioDevice)); }
extern "C" void ee_screenscpp_gt_default_audio_device(void* inOut)
{ EEShimOut(inOut, UWideString(EEGamePreferences::sDefaultSettings.mAudioDevice)); }
extern "C" void ee_screenscpp_gt_game_version(void* inOut) { EEShimOutC(inOut, (const char*)gGameVersion); }
extern "C" void ee_screenscpp_gt_player_name(void* inOut) { EEShimOut(inOut, UWideString(gPlayerName)); }
extern "C" void ee_screenscpp_gt_campaign_roman(void* inOut) { EEShimOut(inOut, UWideString(gCampaignAOCRoman)); }
extern "C" void ee_screenscpp_gt_campaign_pacific(void* inOut) { EEShimOut(inOut, UWideString(gCampaignAOCPacific)); }
extern "C" void ee_screenscpp_gt_campaign_asian(void* inOut) { EEShimOut(inOut, UWideString(gCampaignAOCAsian)); }
extern "C" void ee_screenscpp_gt_campaign_learning(void* inOut) { EEShimOut(inOut, UWideString(gCampaignEELearning)); }
extern "C" void ee_screenscpp_gt_saved_game_extension(void* inOut)
{ EEShimOut(inOut, UWideString(gSavedGameExtension)); }
extern "C" void ee_screenscpp_gt_scenario_extension(void* inOut) { EEShimOut(inOut, UWideString(gScenarioExtension)); }
extern "C" void ee_screenscpp_gt_campaign_extension(void* inOut) { EEShimOut(inOut, UWideString(gCampaignExtension)); }
extern "C" void ee_screenscpp_gt_u530bc8(void* inOut) { EEShimOut(inOut, UWideString(gU530BC8)); }
extern "C" void ee_screenscpp_gt_campaign_error(void* inOut) { EEShimOut(inOut, sCampaignError); }
extern "C" void ee_screenscpp_gt_load_saved_game_name(void* inOut)
{ EEShimOut(inOut, UWideString(gLoadSavedGameName)); }
extern "C" void ee_screenscpp_gt_load_scenario_name(void* inOut) { EEShimOut(inOut, UWideString(gLoadScenarioName)); }
extern "C" void ee_screenscpp_gt_lobby_missing(void* inOut) { EEShimOut(inOut, UWideString(sLobbyMissing)); }
extern "C" void ee_screenscpp_gt_campaign_editor_translated(void* inOut)
{ EEShimOut(inOut, UWideString(sCampaignEditorTranslated)); }
extern "C" void ee_screenscpp_gt_campaign_description(void* inOut)
{ EEShimOut(inOut, UWideString(EECampaignFormManager::Get()->mCampaign.GetDescription())); }
extern "C" void ee_screenscpp_gt_campaign_civilization(void* inOut)
{ EEShimOut(inOut, UWideString(EECampaignFormManager::Get()->mCampaign.GetRequiredCiv())); }
extern "C" void ee_screenscpp_gt_campaign_name(void* inOut)
{ EEShimOut(inOut, UWideString(EECampaignFormManager::Get()->mCampaignName)); }
extern "C" void ee_screenscpp_gt_campaign_form_description(void* inOut)
{ EEShimOut(inOut, UWideString(EECampaignFormManager::Get()->mDescription)); }
extern "C" void ee_screenscpp_gt_intro_briefing(void* inOut)
{ EEShimOut(inOut, UWideString(EEUISCIFormManager::Get()->GetBriefing())); }
extern "C" void ee_screenscpp_gt_intro_hints(void* inOut)
{ EEShimOut(inOut, UWideString(EEUISCIFormManager::Get()->GetHints())); }
extern "C" void ee_screenscpp_gt_intro_history(void* inOut)
{ EEShimOut(inOut, UWideString(EEUISCIFormManager::Get()->GetHistory())); }
extern "C" uintptr_t ee_screenscpp_gt_player(uint32_t inIndex) { return (uintptr_t)EEScreensViewPlayer(inIndex); }
extern "C" void ee_screenscpp_gt_player_name_of(uintptr_t inPlayer, void* inOut)
{ EEShimOut(inOut, UWideString(reinterpret_cast<EEPlayer*>(inPlayer)->GetName())); }
extern "C" void ee_screenscpp_gt_player_civilization(uintptr_t inPlayer, void* inOut)
{ EEShimOut(inOut, UWideString(reinterpret_cast<EEPlayer*>(inPlayer)->mCivilization.mName)); }
extern "C" void ee_screenscpp_gt_world_campaign_name(void* inOut)
{ EEShimOut(inOut, UWideString(gWorld.mStatistics.mCampaignName)); }
extern "C" uint32_t ee_screenscpp_gt_record_count() { return (uint32_t)gWorld.mStatistics.GetRecords().size(); }
extern "C" void ee_screenscpp_gt_record_name(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, UWideString(gWorld.mStatistics.GetRecords()[inIndex].GetName())); }
extern "C" void ee_screenscpp_gt_map_type(uintptr_t inOptions, void* inOut)
{ EEShimOut(inOut, UWideString(GOPT(inOptions)->mMapType)); }

// TEST ONLY: the world lock, for the SGVW trace's note of the reading thread's holds (ee_game screens/sgvw.rs)
extern "C" const void* ee_screenscpp_sgvw_world_lock() { return gWorld.mLock; }
