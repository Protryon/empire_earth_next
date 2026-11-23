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
#include "rust/game/screens.h"
#include "rust/sim/units.h"
#include "ee/EEScreensShims.h"
#include "lle/UITextureBackground.h"

extern float gCameraMaxZoom;   // port: EECameraControl.cpp (the farther zoom-out; 1.0 in the original)
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
unsigned long EEMenuWidth();   // Empire Earth.cpp
unsigned long EEMenuHeight();

// 0x562fd8 (EEUserInterface.cpp keeps the WON password in it): stored in EEGlobals.cpp (ee/EEGlobalStorage.h),
// constructed here, where its definition was
EE_CONSTRUCT_GLOBAL(UString, gMultiplayerString, (""));
EEUIFMainGame* gSavedMainGameForm;          // 0x563144 (.bss among the UI events' statics; ee/EmpireEarth.h)

// (EEScreenEvent::ProcessEvent, EECreateScreenEvent, the event group's ee_screenscpp_event and the grab scroll's
// events: UI Events/EEScreenEvent.cpp)


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

// the delete question, answered yes: the list's selected file deleted
static void EEScreensDeleteSelectedFile(UIForm* inList)
{
    reinterpret_cast<UIFileListBox*>(inList)->DeleteSelectedFile();
}

// the overwrite question, answered no: the pending save dropped
static void EEScreensDropSave(UIForm* inObject)
{
    reinterpret_cast<UReferenceCounter*>(inObject)->Dereference();
}

// ---- the campaign scenario select and scenario intro forms' (scenario_screens.rs)


// the intro's scenario file (GetScenario's, for Start's load or game)
static FSFileSpec& EEScreensScenario() { return EEScratch<FSFileSpec>(kScratchScenario); }

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

// ---- the lobby's and its load screens' (lobby_form.rs)

#define GOPT(inOptions) reinterpret_cast<EEGameOptions*>(inOptions)
#define GOP(inPlayer) reinterpret_cast<EEGameOptionsPlayer*>(inPlayer)
// the saved game checks' (lobby_calls.rs lobby_load; the original EEScreensLobbyLoad's locals): the start check's
// file, the header's options, the lobby options with the load type, the missing lines



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


static UWideString Text(const uint8_t* inText, size_t inLength)
{
    return UWideString(std::string((const char*)inText, inLength).c_str());
}

// ---- the hot keys' (hot_keys.rs): inForm is the main game form

// the last snapshot's complex units (EE_SCREENS_GAME_HK_CENTER's record index), kept by ee_screenscpp_hot_key_keep

extern "C" void ee_screenscpp_hk_target_reset() { EEScratch<GE3DPoint>(kScratchHotKeyTarget) = GE3DPoint(); }
extern "C" bool ee_screenscpp_hk_location_at()
{ return gWorld.mMap->GetLocationAt(GE2DPoint(gU4E8F08.GetXCenter(), gU4E8F08.GetYCenter()), EEScratch<GE3DPoint>(kScratchHotKeyTarget), true) != NULL; }
extern "C" void ee_screenscpp_hk_tile_at(long* inX, long* inY)
{ gWorld.mMap->GetTileAt(GE2DPoint(gU4E8F08.GetXCenter(), gU4E8F08.GetYCenter()), *inX, *inY); }
extern "C" float ee_screenscpp_hk_target_x() { return EEScratch<GE3DPoint>(kScratchHotKeyTarget).GetX(); }
extern "C" float ee_screenscpp_hk_target_y() { return EEScratch<GE3DPoint>(kScratchHotKeyTarget).GetY(); }
extern "C" void ee_screenscpp_hk_units_clear() { EEScratch<std::vector<EEComplexUnit*>>(kScratchHotKeyUnits).clear(); }
extern "C" void ee_screenscpp_hk_units_push(uint64_t inUnit)
{ EEScratch<std::vector<EEComplexUnit*>>(kScratchHotKeyUnits).push_back(reinterpret_cast<EEComplexUnit*>((uintptr_t)inUnit)); }
extern "C" uintptr_t ee_screenscpp_sel_control(uintptr_t inForm, uint32_t inControl)
{ return (uintptr_t)static_cast<UIForm*>(reinterpret_cast<void*>(inForm))->GetControl(inControl); }
extern "C" int32_t ee_screenscpp_sel_list(uintptr_t inControl, unsigned long* inSelection)
{ return (int32_t)static_cast<UILBString*>(reinterpret_cast<UIControl*>(inControl))->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_sel_combo(uintptr_t inControl, unsigned long* inSelection)
{ return (int32_t)static_cast<UIComboBox*>(reinterpret_cast<UIControl*>(inControl))->GetCurrentSelection(*inSelection); }

// ---- the chat log (chat_log.rs): its C++ entry points are inline (ee/EEUIEChatLogEvents.h); a saved game's reads
// and writes


#define CL_FILE(x) (*reinterpret_cast<FSFile*>(x))
extern "C" int32_t ee_screenscpp_cl_read_ulong(uintptr_t inFile, unsigned long* inValue)
{ return (int32_t)CL_FILE(inFile).Read(*inValue); }
extern "C" int32_t ee_screenscpp_cl_read_string(uintptr_t inFile)
{ return (int32_t)EEScratch<UWideString>(kScratchChatLogString).Load(CL_FILE(inFile)); }
extern "C" int32_t ee_screenscpp_cl_read_colour(uintptr_t inFile)
{ return (int32_t)EEScratch<GEColor>(kScratchChatLogColour).Load(CL_FILE(inFile)); }
extern "C" void ee_screenscpp_cl_string(void* inText) { EEShimOut(inText, EEScratch<UWideString>(kScratchChatLogString)); }
extern "C" float ee_screenscpp_cl_red() { return EEScratch<GEColor>(kScratchChatLogColour).mRed; }
extern "C" float ee_screenscpp_cl_green() { return EEScratch<GEColor>(kScratchChatLogColour).mGreen; }
extern "C" float ee_screenscpp_cl_blue() { return EEScratch<GEColor>(kScratchChatLogColour).mBlue; }
extern "C" void ee_screenscpp_cl_set_red(float inV) { EEScratch<GEColor>(kScratchChatLogColour).mRed = inV; }
extern "C" void ee_screenscpp_cl_set_green(float inV) { EEScratch<GEColor>(kScratchChatLogColour).mGreen = inV; }
extern "C" void ee_screenscpp_cl_set_blue(float inV) { EEScratch<GEColor>(kScratchChatLogColour).mBlue = inV; }
extern "C" int32_t ee_screenscpp_cl_write_ulong(uintptr_t inFile, uint32_t inValue)
{ return (int32_t)CL_FILE(inFile).Write(EEScratch<unsigned long>(kScratchChatLogValue) = inValue); }
extern "C" int32_t ee_screenscpp_cl_write_string(uintptr_t inFile, EeScreensShimText inText)
{ return (int32_t)EEShimText(inText).Save(CL_FILE(inFile)); }
extern "C" int32_t ee_screenscpp_cl_write_colour(uintptr_t inFile)
{ return (int32_t)EEScratch<GEColor>(kScratchChatLogColour).Save(CL_FILE(inFile)); }

// ---- the civilization builder's (civ_builder.rs): inForm is the builder's form or one of its dialogs


// ---- the camera, scroll and zoom events' (camera.rs): inForm is the main game form or the scenario editor's

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

// ---- the post-game summary's (post_game.rs): inForm is the summary's form (25)

// the post-game line graph's x values: the game's time (ee_screens post_game_ffi.rs ee_screens_pgs_format)
extern "C" uint32_t ee_screenscpp_game_time() { return (uint32_t)gPhysics->GetGameTime(); }

// ---- the civilization choice form's (civ_choice.rs): the form 35 by the manager, or the event's form

static EECBUIListBox* EEScreensPdcsControl(unsigned long inIndex)
{
    return static_cast<EECBUIListBox*>(gUIManager->GetFormByID(kFormPredefinedCiv)->GetControl(inIndex));
}

// ---- the debug sound panel's (debug_sound.rs): the first selected unit's sound of a kind
#define DSUC() EEScratch<EEComplexUnit*>(kScratchDsuUnit)->mComplexUnit->mUnitClass

// ---- the campaign editor's (campaign_editor.rs): inForm is the editor's form (24); its model is Rust's
// (campaign_model.rs: the original's EECampaignEditorBehavior gCampaignEditor, 0x562df0), over the edited campaign


// The model's host's state (campaign_model_calls.rs, EE_SCREENS_CEM_*): the edited campaign, the strings and the
// files (the original model's calls: Campaign/EECampaignEditorBehavior.cpp)

// (the model's host, the original's ee_screenscpp_ce_campaign and _text, is Rust's over the shims below:
// ee_screens campaign_model_calls.rs; each one call, no logic)
#define CEC EEScratch<EECampaign>(kScratchCampaignEditorCampaign)
extern "C" void ee_screenscpp_cem_kept_clear() { EEScratch<UWideString>(kScratchCampaignEditorKept) = UWideString(); }
extern "C" uint32_t ee_screenscpp_cem_string(uint32_t inId)
{ return (uint32_t)gResource->GetString(inId, EEScratch<UWideString>(kScratchCampaignEditorKept)); }
extern "C" void ee_screenscpp_cem_set_name(EeScreensShimText inText) { CEC.SetCampaignName(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_author(EeScreensShimText inText) { CEC.SetAuthorName(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_email(EeScreensShimText inText) { CEC.SetAuthorEmail(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_background(EeScreensShimText inText)
{ CEC.SetBackgroundImage(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_civilization(EeScreensShimText inText) { CEC.SetRequiredCiv(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_description(EeScreensShimText inText)
{ CEC.SetCampaignDescription(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_password(EeScreensShimText inText) { CEC.SetPassword(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_scenarios_clear() { EEScratch<std::vector<UWideString>>(kScratchCEMScenarios).clear(); }
extern "C" void ee_screenscpp_cem_scenarios_add(EeScreensShimText inText)
{ EEScratch<std::vector<UWideString>>(kScratchCEMScenarios).push_back(EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_set_scenarios() { CEC.SetCampaignScenarios(EEScratch<std::vector<UWideString>>(kScratchCEMScenarios)); }
extern "C" uint32_t ee_screenscpp_cem_load(EeScreensShimText inFile)
{ return (uint32_t)CEC.Load(EEShimText(inFile), EEScratch<UWideString>(kScratchCampaignEditorKept), true); }
extern "C" uint32_t ee_screenscpp_cem_save() { return (uint32_t)CEC.Save(EEScratch<UWideString>(kScratchCampaignEditorKept)); }
extern "C" void ee_screenscpp_cem_lines_clear() { EEScratch<std::vector<UWideString>>(kScratchCampaignEditorLines).clear(); }
extern "C" uint32_t ee_screenscpp_cem_list()
{ return (uint32_t)CEC.ListScenarioFiles(EEScratch<std::vector<UWideString>>(kScratchCampaignEditorLines), EEScratch<UWideString>(kScratchCampaignEditorKept), false, false); }
extern "C" void ee_screenscpp_cem_delete_reset() { EEScratch<FSFileSpec>(kScratchCEMDelete) = FSFileSpec(); }
extern "C" void ee_screenscpp_cem_delete_volume() { EEScratch<FSFileSpec>(kScratchCEMDelete).SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cem_delete_directory()
{ EEScratch<FSFileSpec>(kScratchCEMDelete).SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\"); }
extern "C" void ee_screenscpp_cem_delete_file(EeScreensShimText inText) { EEScratch<FSFileSpec>(kScratchCEMDelete).SetFile(EEShimString(inText)); }
extern "C" bool ee_screenscpp_cem_delete() { return EEScratch<FSFileSpec>(kScratchCEMDelete).Delete() == kErrorOK; }
extern "C" void ee_screenscpp_cem_files_clear() { EEScratch<std::set<UString>>(kScratchCampaignEditorFiles).clear(); }
extern "C" void ee_screenscpp_cem_spec_clear() { EEScratch<FSFileSpec*>(kScratchCampaignEditorSpec) = NULL; }
extern "C" void ee_screenscpp_cem_file_clear() { EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile) = NULL; }
extern "C" void ee_screenscpp_cem_path_new(uint32_t inIndex) { EEScratch<FSPath*[2]>(kScratchCampaignEditorPaths)[inIndex] = new FSPath; }
extern "C" void ee_screenscpp_cem_set_directory() { CEC.SetDirectory((const char*)(gStartupDirectory + "data\\")); }
extern "C" void ee_screenscpp_cem_directory_volume() { EEScratch<FSFileSpec>(kScratchCampaignEditorDirectory).SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cem_directory_scenarios()
{ EEScratch<FSFileSpec>(kScratchCampaignEditorDirectory).SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\"); }
extern "C" void ee_screenscpp_cem_directory_campaigns()
{ EEScratch<FSFileSpec>(kScratchCampaignEditorDirectory).SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\"); }
extern "C" void ee_screenscpp_cem_path_add(uint32_t inIndex)
{ EEScratch<FSPath*[2]>(kScratchCampaignEditorPaths)[inIndex]->AddDirectory(EEScratch<FSFileSpec>(kScratchCampaignEditorDirectory), true); }
extern "C" void ee_screenscpp_cem_list_scenario_files()
{ EEScratch<FSPath*[2]>(kScratchCampaignEditorPaths)[1]->ListFiles(UString("*") + gScenarioExtension, EEScratch<std::set<UString>>(kScratchCampaignEditorFiles)); }
extern "C" void ee_screenscpp_cem_list_campaign_files()
{ EEScratch<FSPath*[2]>(kScratchCampaignEditorPaths)[0]->ListFiles(UString("*") + gCampaignExtension, EEScratch<std::set<UString>>(kScratchCampaignEditorFiles)); }
extern "C" uint32_t ee_screenscpp_cem_built_in(EeScreensShimText inText)
{ return (uint32_t)EECampaignFormManager::GetBuiltInCampaign(EEShimText(inText)); }
extern "C" int64_t ee_screenscpp_cem_find_campaign()
{ return EEScratch<FSPath*[2]>(kScratchCampaignEditorPaths)[0]->FindFile(UString((const char*)(CEC.GetCampaignName() + (const char*)gCampaignExtension)), EEScratch<FSFileSpec*>(kScratchCampaignEditorSpec)); }
extern "C" uintptr_t ee_screenscpp_cem_archive_new() { return (uintptr_t)new FSArchive(*EEScratch<FSFileSpec*>(kScratchCampaignEditorSpec)); }
extern "C" int64_t ee_screenscpp_cem_archive_open(uintptr_t inArchive)
{ return reinterpret_cast<FSArchive*>(inArchive)->Open(); }
extern "C" int64_t ee_screenscpp_cem_archive_find(uintptr_t inArchive, EeScreensShimText inText)
{ return reinterpret_cast<FSArchive*>(inArchive)->FindFile("Scenarios\\" + gU52A280 + EEShimText(inText) + (const char*)gScenarioExtension, EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile)); }
extern "C" void ee_screenscpp_cem_archive_close(uintptr_t inArchive)
{ reinterpret_cast<FSArchive*>(inArchive)->Close(); }
extern "C" void ee_screenscpp_cem_archive_delete(uintptr_t inArchive)
{ delete reinterpret_cast<FSArchive*>(inArchive); }
extern "C" int64_t ee_screenscpp_cem_file_open() { return EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile)->Open(kFileAccessRead); }
extern "C" int64_t ee_screenscpp_cem_file_length(unsigned long* inLength)
{ return EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile)->GetLength(*inLength); }
extern "C" void ee_screenscpp_cem_file_read() { EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile)->Read(EEScratch<unsigned char>(kScratchCEMByte)); }
extern "C" uint32_t ee_screenscpp_cem_file_byte() { return EEScratch<unsigned char>(kScratchCEMByte); }
extern "C" uint64_t ee_screenscpp_cem_file_checksum(EeScreensShimText inText)
{ return (uint64_t)EEGetFileChecksum(EEScratch<FSFileSpec>(kScratchCampaignEditorDirectory), UString((const char*)(EEShimText(inText) + (const char*)gScenarioExtension)), 0); }
extern "C" void ee_screenscpp_cem_differs_format()
{ gResource->GetString(IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS, EEScratch<UWideString>(kScratchCEMFormat)); }
extern "C" void ee_screenscpp_cem_differs_print(EeScreensShimText inText)
{ sprintf(EEScratch<UWideString>(kScratchCEMMessage), (const char*)EEScratch<UWideString>(kScratchCEMFormat), (const char*)CEC.GetCampaignName(), (const char*)EEShimText(inText)); }
extern "C" void ee_screenscpp_cem_differs_show() { gUIManager->DisplayMessage(EEScratch<UWideString>(kScratchCEMMessage), true, NULL, NULL, true); }
extern "C" void ee_screenscpp_cem_file_close() { EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile)->Close(); }
extern "C" void ee_screenscpp_cem_file_delete() { delete EEScratch<FSMemoryFile*>(kScratchCampaignEditorFile); }
extern "C" void ee_screenscpp_cem_path_dereference(uint32_t inIndex) { EEScratch<FSPath*[2]>(kScratchCampaignEditorPaths)[inIndex]->Dereference(); }
extern "C" void ee_screenscpp_cem_spec_delete() { delete EEScratch<FSFileSpec*>(kScratchCampaignEditorSpec); }
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
extern "C" void ee_screenscpp_cem_kept(void* inOut) { EEShimOut(inOut, EEScratch<UWideString>(kScratchCampaignEditorKept)); }
extern "C" uint32_t ee_screenscpp_cem_line_count() { return (uint32_t)EEScratch<std::vector<UWideString>>(kScratchCampaignEditorLines).size(); }
extern "C" void ee_screenscpp_cem_line(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, EEScratch<std::vector<UWideString>>(kScratchCampaignEditorLines)[inIndex]); }
extern "C" void ee_screenscpp_cem_files_snapshot()
{ EEScratch<std::vector<UString>>(kScratchCEMFileList).assign(EEScratch<std::set<UString>>(kScratchCampaignEditorFiles).begin(), EEScratch<std::set<UString>>(kScratchCampaignEditorFiles).end()); }
extern "C" uint32_t ee_screenscpp_cem_file_count() { return (uint32_t)EEScratch<std::vector<UString>>(kScratchCEMFileList).size(); }
extern "C" void ee_screenscpp_cem_file_name(uint32_t inIndex, void* inOut) { EEShimOut(inOut, EEScratch<std::vector<UString>>(kScratchCEMFileList)[inIndex]); }
extern "C" void ee_screenscpp_cem_scenario_extension(void* inOut) { EEShimOut(inOut, gScenarioExtension); }
extern "C" void ee_screenscpp_cem_campaign_extension(void* inOut) { EEShimOut(inOut, gCampaignExtension); }

// ---- the shims of the moved cases (UI lane round 7: rust/crates/ee_game/src/screens/game_calls.rs, game_cpp.rs):
// each one C++ statement; the logic that calls them is Rust's
static UIForm* F(uintptr_t inForm) { return reinterpret_cast<UIForm*>(inForm); }
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
extern "C" void ee_screenscpp_civ_clear_upgrades() { EEScratch<EECivilization>(kScratchCivilization).mPurchasedUpgrades.clear(); }
extern "C" void ee_screenscpp_civ_reset_points() { EEScratch<EECivilization>(kScratchCivilization).mAvailablePoints = EEScratch<EECivilization>(kScratchCivilization).mMaxPoints; }
extern "C" int32_t ee_screenscpp_civ_builder_points()
{ return (int32_t)gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints); }
extern "C" void ee_screenscpp_civ_set_points_and_max(int32_t inPoints) { EEScratch<EECivilization>(kScratchCivilization).SetPointsAndMax(inPoints); }
extern "C" void ee_screenscpp_open_civ_builder() { gUserInterface->OpenCivilizationBuilder(&EEScratch<EECivilization>(kScratchCivilization), false); }
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
extern "C" void ee_screenscpp_won_info_reset() { EEScratch<EEWonNetGameInfo>(kScratchWonInfo) = EEWonNetGameInfo(); }
extern "C" void ee_screenscpp_lobby_start_time_now() { EEScratch<unsigned long>(kScratchLobbyStartTime) = TMGetTime(); }
extern "C" void ee_screenscpp_server_allow_new_players(bool inAllow)
{ static_cast<EEServer*>(gCommunications)->AllowNewPlayers(inAllow); }
extern "C" void ee_screenscpp_server_set_game_state(uint32_t inState)
{ static_cast<EEServer*>(gCommunications)->SetGameState(inState); }
extern "C" void ee_screenscpp_won_info_load() { gWonNet->GetGameInfo(EEScratch<EEWonNetGameInfo>(kScratchWonInfo)); }
extern "C" uint32_t ee_screenscpp_won_info_flags() { return (uint32_t)EEScratch<EEWonNetGameInfo>(kScratchWonInfo).mFlags; }
extern "C" void ee_screenscpp_won_info_set_flags(uint32_t inFlags) { EEScratch<EEWonNetGameInfo>(kScratchWonInfo).mFlags = inFlags; }
extern "C" void ee_screenscpp_won_info_set_players(uint8_t inPlayers) { EEScratch<EEWonNetGameInfo>(kScratchWonInfo).mPlayers = inPlayers; }
extern "C" void ee_screenscpp_won_info_set_humans(uint8_t inHumans) { EEScratch<EEWonNetGameInfo>(kScratchWonInfo).mHumans = inHumans; }
extern "C" uint8_t ee_screenscpp_won_info_players() { return EEScratch<EEWonNetGameInfo>(kScratchWonInfo).mPlayers; }
extern "C" uint8_t ee_screenscpp_won_info_humans() { return EEScratch<EEWonNetGameInfo>(kScratchWonInfo).mHumans; }
extern "C" void ee_screenscpp_won_info_store() { gWonNet->SetGameInfo(EEScratch<EEWonNetGameInfo>(kScratchWonInfo)); }
extern "C" void ee_screenscpp_server_reset_xfer() { static_cast<EEServer*>(gCommunications)->ResetXfer(); }
extern "C" void ee_screenscpp_send_full_player(uint32_t inIndex)
{ gCommunications->ProcessGuaranteedLocalUserInput(EENewFullPlayerInformation(gGameOptions.mPlayers[inIndex]), kSlotNone); }
extern "C" int32_t ee_screenscpp_find_generator(uintptr_t inOptions, unsigned long* inLine)
{ return (int32_t)EERMFindGenerator(GOPT(inOptions)->GetMapType(), *inLine); }
extern "C" uintptr_t ee_screenscpp_lobby_boot() { return (uintptr_t)EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot); }
extern "C" void ee_screenscpp_server_refuse_boot(uintptr_t inServer)
{ reinterpret_cast<EEServer*>(inServer)->RefuseClient(*EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot)); }
extern "C" bool ee_screenscpp_lobby_has_file() { return EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader) && EEScratch<FSFileSpec*>(kScratchLobbyFile); }
extern "C" void ee_screenscpp_server_abort_xfer(uint32_t inId)
{ static_cast<NESynchronousServer*>(gCommunications)->AbortXfer(inId); }
extern "C" void ee_screenscpp_server_xfer_lobby_file(uint8_t inSlot, unsigned long* inId)
{ static_cast<NESynchronousServer*>(gCommunications)->XferFileToClient(*EEScratch<FSFileSpec*>(kScratchLobbyFile), EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader)->mChecksum, static_cast<EEServer*>(gCommunications)->GetSendToSlot(inSlot), *inId, false); }
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
extern "C" void ee_screenscpp_campaign_error_reset() { EEScratch<UWideString>(kScratchCampaignError) = UWideString(); }
extern "C" bool ee_screenscpp_campaign_load(EeScreensShimText inFile)
{ return EECampaignFormManager::Get()->mCampaign.Load(EEShimText(inFile), EEScratch<UWideString>(kScratchCampaignError), false); }
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
extern "C" void ee_screenscpp_shim_message_reset() { EEScratch<UWideString>(kScratchShimMessage) = UWideString(); }
extern "C" bool ee_screenscpp_validate_save_game(uintptr_t inSpec)
{ return EEValidateSaveGameFile(*S(inSpec), EEScratch<UWideString>(kScratchShimMessage)); }
extern "C" void ee_screenscpp_display_shim_message() { gUIManager->DisplayMessage(EEScratch<UWideString>(kScratchShimMessage), true, NULL, 0, true); }
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
extern "C" void ee_screenscpp_lobby_header_delete() { delete EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader); }
extern "C" void ee_screenscpp_lobby_header_new() { EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader) = new EEPSaveGameHeader; }
extern "C" void ee_screenscpp_lobby_file_clear() { EEScratch<FSFileSpec*>(kScratchLobbyFile) = NULL; }
extern "C" uintptr_t ee_screenscpp_debug_sound_label(uint32_t inId)
{ return (uintptr_t)static_cast<UIControl*>(dynamic_cast<UITextLabel*>(gUIManager->GetFormByID(kFormDebugSound)->GetControlById(inId))); }
extern "C" void ee_screenscpp_label_set_text(uintptr_t inLabel, EeScreensShimText inText)
{ static_cast<UITextLabel*>(C(inLabel))->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_pgs_send(EeScreensShimText inHost, uint32_t inPort, EeScreensShimText inPayload)
{ EEScratch<NEProtocol*>(kScratchPgsProtocol)->SendSingle(EEShimString(inHost), (unsigned short)inPort, inPayload.ptr, (unsigned long)inPayload.len); }
extern "C" void ee_screenscpp_pdcs_name_reset() { EEScratch<UWideString>(kScratchPdcsName) = UWideString(); }
extern "C" void ee_screenscpp_pdcs_upgrades_clear() { EEScratch<std::vector<EECivUpgrade>>(kScratchPdcsUpgrades).clear(); }
extern "C" uint32_t ee_screenscpp_pdcs_load(EeScreensShimText inFile)
{ return (uint32_t)EELoadCivilization(EEShimString(inFile), EEGetCivilizationDirectory(false), EEScratch<UWideString>(kScratchPdcsName), EEScratch<std::vector<EECivUpgrade>>(kScratchPdcsUpgrades)); }

// shims3 (game_calls_more.rs)
extern "C" uintptr_t ee_screenscpp_player_event_new() { return (uintptr_t)static_cast<EEAction*>(new EEAPlayerEvent); }
extern "C" float ee_screenscpp_hot_key_unit_x(uint32_t inIndex)
{ return EEScratch<std::vector<EEComplexUnit*>>(kScratchHotKeyUnits)[inIndex]->mPhysicalModel.GetLocation().GetX(); }
extern "C" float ee_screenscpp_hot_key_unit_y(uint32_t inIndex)
{ return EEScratch<std::vector<EEComplexUnit*>>(kScratchHotKeyUnits)[inIndex]->mPhysicalModel.GetLocation().GetY(); }
extern "C" float ee_screenscpp_hot_key_unit_z(uint32_t inIndex)
{ return EEScratch<std::vector<EEComplexUnit*>>(kScratchHotKeyUnits)[inIndex]->mPhysicalModel.GetLocation().GetZ(); }
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
extern "C" void ee_screenscpp_protocol_clear() { EEScratch<NEProtocol*>(kScratchShimProtocol) = NULL; }
extern "C" void ee_screenscpp_protocol_load() { NEProtocol::GetProtocol(0, EEScratch<NEProtocol*>(kScratchShimProtocol)); }
extern "C" void ee_screenscpp_world_move_player_to_editor()
{ gWorld.MovePlayerToSlot(0xfe, gWorld.GetCurrentPlayer()->GetSlot(), 1, EEScratch<NEProtocol*>(kScratchShimProtocol)->GetLocalAddress(gNetworkAdapter), 0); }
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
{ return (uintptr_t)gUIManager->GetMainGameForm()->mMainGroup->GetSelectedUnit(); }
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
{ return (uintptr_t)static_cast<UIControl*>(new UIStatusBar(inId, GERectangle(EEScratch<float[4]>(kScratchPgsRectangle)[0], EEScratch<float[4]>(kScratchPgsRectangle)[1], EEScratch<float[4]>(kScratchPgsRectangle)[2], EEScratch<float[4]>(kScratchPgsRectangle)[3]), F(inForm), inIndex, inBorder)); }
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
{ EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->SetFirstScenario(EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->mCampaign.GetRequiredCiv()); }
extern "C" void ee_screenscpp_pgs_campaign_description()
{ EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->SetDescription(EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->mCampaign.GetDescription()); }
extern "C" bool ee_screenscpp_pgs_campaign_is_demo()
{ return EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->mCampaignName.EqualsIgnoreCase(gCampaignEEDemo); }
extern "C" bool ee_screenscpp_pgs_campaign_is_learning()
{ return EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->mCampaignName.EqualsIgnoreCase(gCampaignEELearning); }
extern "C" void ee_screenscpp_pgs_campaign_set_all_open(bool inOpen) { EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->mAllScenariosOpen = inOpen; }
extern "C" void ee_screenscpp_pgs_campaign_set_name(EeScreensShimText inName)
{ EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->SetCampaignName(EEShimText(inName)); }
extern "C" void ee_screenscpp_pgs_campaign_set_first(EeScreensShimText inName)
{ EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->SetFirstScenario(EEShimText(inName)); }
extern "C" void ee_screenscpp_pgs_campaign_set_description(EeScreensShimText inText)
{ EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->SetDescription(EEShimText(inText)); }
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
extern "C" void ee_screenscpp_pdcs_civs_clear() { EEScratch<std::vector<EECivilizationEntry>>(kScratchPdcsCivs).clear(); }
extern "C" void ee_screenscpp_pdcs_civs_get() { gCivilizationManager.GetCivilizations(EEScratch<std::vector<EECivilizationEntry>>(kScratchPdcsCivs)); }
extern "C" uint32_t ee_screenscpp_pdcs_civs_count() { return (uint32_t)EEScratch<std::vector<EECivilizationEntry>>(kScratchPdcsCivs).size(); }
extern "C" uintptr_t ee_screenscpp_civ_request_new()
{ return (uintptr_t)static_cast<NEMessage*>(new EEMRecord(kMessageCivUpgradesRequested, {gWorld.mCurrentPlayer, 0, EEScratch<std::vector<EECivUpgrade>>(kScratchPdcsUpgrades), EEScratch<UWideString>(kScratchPdcsName)})); }
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
extern "C" bool ee_screenscpp_lobby_copy_exists() { return EEScratch<EEGameOptions*>(kScratchLobbyCopy) != NULL; }
extern "C" void ee_screenscpp_lobby_copy_new() { EEScratch<EEGameOptions*>(kScratchLobbyCopy) = new EEGameOptions; }
extern "C" void ee_screenscpp_lobby_copy_from_options() { *EEScratch<EEGameOptions*>(kScratchLobbyCopy) = gGameOptions; }
extern "C" void ee_screenscpp_lobby_copy_clear_load_type() { EEScratch<EEGameOptions*>(kScratchLobbyCopy)->mLoadType = 0; }
extern "C" void ee_screenscpp_options_from_lobby_copy() { gGameOptions = *EEScratch<EEGameOptions*>(kScratchLobbyCopy); }
// end shims3

// ---- the lobby's sources, slots, options and saved game checks (lobby_calls.rs; each one statement)
extern "C" uintptr_t ee_screenscpp_lobby_header() { return (uintptr_t)EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader); }
extern "C" uintptr_t ee_screenscpp_lobby_header_options(uintptr_t inHeader)
{ return (uintptr_t)&reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mGameOptions; }
extern "C" uintptr_t ee_screenscpp_lobby_copy() { return (uintptr_t)EEScratch<EEGameOptions*>(kScratchLobbyCopy); }
extern "C" uintptr_t ee_screenscpp_game_options_ptr() { return (uintptr_t)&gGameOptions; }
extern "C" uintptr_t ee_screenscpp_options_player(uintptr_t inOptions, uint32_t inIndex)
{ return (uintptr_t)&GOPT(inOptions)->mPlayers[inIndex]; }
extern "C" void ee_screenscpp_lobby_boot_new() { EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot) = new EEGameOptionsPlayer; }
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
// the lobby options (ee_screens_lobby_set_option, lobby_form_ffi.rs, sets them)
extern "C" EeAppOptionsFields* ee_screenscpp_lobby_options() { return &gGameOptions; }
extern "C" void ee_screenscpp_send_full_options()
{ gCommunications->ProcessGuaranteedLocalUserInput(EENewFullGameOptions(gGameOptions), kSlotNone); }
extern "C" void ee_screenscpp_lobby_boot_delete() { delete EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot); }
extern "C" void ee_screenscpp_lobby_boot_clear() { EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot) = NULL; }
extern "C" void ee_screenscpp_world_lobby_single() { gWorld.GetLobbyOptions().mIsMultiplayer = false; }
extern "C" void ee_screenscpp_lobby_player_set_name(uint32_t inSlot, EeScreensShimText inName)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mName = EEShimText(inName); }
extern "C" int64_t ee_screenscpp_ll_open_lobby(EeScreensShimText inFile, EeScreensShimText inDirectory)
{ return EEOpenSaveGameFile(EEShimString(inFile), EEShimString(inDirectory), EEScratch<FSFileSpec*>(kScratchLobbyFile)); }
extern "C" uintptr_t ee_screenscpp_lobby_file() { return (uintptr_t)EEScratch<FSFileSpec*>(kScratchLobbyFile); }
extern "C" void ee_screenscpp_ll_check_clear() { EEScratch<FSFileSpec*>(kScratchLLCheck) = NULL; }
extern "C" int64_t ee_screenscpp_ll_open_check(EeScreensShimText inFile, EeScreensShimText inDirectory)
{ return EEOpenSaveGameFile(EEShimString(inFile), EEShimString(inDirectory), EEScratch<FSFileSpec*>(kScratchLLCheck)); }
extern "C" uintptr_t ee_screenscpp_ll_check_file() { return (uintptr_t)EEScratch<FSFileSpec*>(kScratchLLCheck); }
extern "C" void ee_screenscpp_ll_options_from_header(uintptr_t inHeader)
{ EEScratch<EEGameOptions>(kScratchLLOptions) = reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mGameOptions; }
extern "C" void ee_screenscpp_ll_missing_clear() { EEScratch<std::vector<UWideString>>(kScratchLLMissing).clear(); }
extern "C" bool ee_screenscpp_ll_check_start(uintptr_t inHeader)
{ return EECheckSavedGameOptions(EEScratch<EEGameOptions>(kScratchLLOptions), gGameOptions, true, EEScratch<std::vector<UWideString>>(kScratchLLMissing), reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mNumPlayers - 1); }
extern "C" void ee_screenscpp_ll_current_from_game() { EEScratch<EEGameOptions>(kScratchLLCurrent) = gGameOptions; }
extern "C" void ee_screenscpp_ll_current_load_type(uint32_t inLoadType) { EEScratch<EEGameOptions>(kScratchLLCurrent).mLoadType = inLoadType; }
extern "C" bool ee_screenscpp_ll_check_load(uintptr_t inHeader)
{ return EECheckSavedGameOptions(EEScratch<EEGameOptions>(kScratchLLOptions), EEScratch<EEGameOptions>(kScratchLLCurrent), false, EEScratch<std::vector<UWideString>>(kScratchLLMissing), reinterpret_cast<EEPSaveGameHeader*>(inHeader)->mNumPlayers - 1); }
extern "C" void ee_screenscpp_lobby_missing_clear() { EEScratch<UWideString>(kScratchLobbyMissing) = ""; }
extern "C" uint32_t ee_screenscpp_ll_missing_count() { return (uint32_t)EEScratch<std::vector<UWideString>>(kScratchLLMissing).size(); }
extern "C" void ee_screenscpp_lobby_missing_add(uint32_t inIndex) { EEScratch<UWideString>(kScratchLobbyMissing) += EEScratch<std::vector<UWideString>>(kScratchLLMissing)[inIndex] + "\n"; }
extern "C" void ee_screenscpp_le_map_display(uintptr_t inOptions, void* inOut)
{ EEShimOut(inOut, UWideString(EERMGetGeneratorDisplayName(GOPT(inOptions)->GetMapType()))); }

// ---- the settings form's records (settings_form.rs)

// the settings the form reads and writes (settings_form_ffi.rs copies their members), the defaults read only
extern "C" EeAppSettingsFields* ee_screenscpp_settings_fields(bool inDefaults)
{ return inDefaults ? &EEGamePreferences::sDefaultSettings : &gGamePreferences.GetSettings(); }

// the settings form's display questions: display_modes.rs over these (UI lane round 12)
extern "C" uintptr_t ee_screenscpp_disp_rasterizer_count() { return gDisplayRasterizers.size(); }
extern "C" uint64_t ee_screenscpp_disp_rasterizer_index(uintptr_t inR)
{ return (uint64_t)gDisplayRasterizers[inR].mIndex; }
extern "C" uintptr_t ee_screenscpp_disp_screen_count(uintptr_t inR) { return gDisplayRasterizers[inR].mScreens.size(); }
extern "C" uint64_t ee_screenscpp_disp_screen_index(uintptr_t inR, uintptr_t inS)
{ return (uint64_t)gDisplayRasterizers[inR].mScreens[inS].mIndex; }
extern "C" uintptr_t ee_screenscpp_disp_mode_count(uintptr_t inR, uintptr_t inS)
{ return gDisplayRasterizers[inR].mScreens[inS].mModes.size(); }
extern "C" uint64_t ee_screenscpp_disp_mode_width(uintptr_t inR, uintptr_t inS, uintptr_t inM)
{ return (uint64_t)gDisplayRasterizers[inR].mScreens[inS].mModes[inM].mMode.mWidth; }
extern "C" uint64_t ee_screenscpp_disp_mode_height(uintptr_t inR, uintptr_t inS, uintptr_t inM)
{ return (uint64_t)gDisplayRasterizers[inR].mScreens[inS].mModes[inM].mMode.mHeight; }
extern "C" uint64_t ee_screenscpp_disp_mode_refresh(uintptr_t inR, uintptr_t inS, uintptr_t inM)
{ return (uint64_t)gDisplayRasterizers[inR].mScreens[inS].mModes[inM].mMode.mRefreshRate; }
extern "C" uint64_t ee_screenscpp_disp_mode_bits(uintptr_t inR, uintptr_t inS, uintptr_t inM)
{ return (uint64_t)gDisplayRasterizers[inR].mScreens[inS].mModes[inM].mMode.mBitCount; }
extern "C" uint64_t ee_screenscpp_disp_current_width(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetScreen(inScreen)->GetCurrentMode().mWidth; }
extern "C" uint64_t ee_screenscpp_disp_current_height(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetScreen(inScreen)->GetCurrentMode().mHeight; }
extern "C" uint64_t ee_screenscpp_disp_current_refresh(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetScreen(inScreen)->GetCurrentMode().mRefreshRate; }
extern "C" uint64_t ee_screenscpp_disp_current_bits(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetScreen(inScreen)->GetCurrentMode().mBitCount; }
extern "C" void ee_screenscpp_disp_game_size(uint32_t inPercent, bool inWindowed, unsigned long* inWidth, unsigned long* inHeight)
{ GXGameSize(inPercent, inWindowed, inWidth, inHeight); }

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
extern "C" bool ee_screenscpp_gv_has_header() { return EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader) != NULL; }
extern "C" uint32_t ee_screenscpp_gv_header_players() { return (uint32_t)EEScratch<EEPSaveGameHeader*>(kScratchLobbyHeader)->mNumPlayers; }
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
extern "C" void ee_screenscpp_gt_campaign_error(void* inOut) { EEShimOut(inOut, EEScratch<UWideString>(kScratchCampaignError)); }
extern "C" void ee_screenscpp_gt_load_saved_game_name(void* inOut)
{ EEShimOut(inOut, UWideString(gLoadSavedGameName)); }
extern "C" void ee_screenscpp_gt_load_scenario_name(void* inOut) { EEShimOut(inOut, UWideString(gLoadScenarioName)); }
extern "C" void ee_screenscpp_gt_lobby_missing(void* inOut) { EEShimOut(inOut, UWideString(EEScratch<UWideString>(kScratchLobbyMissing))); }
extern "C" void ee_screenscpp_gt_campaign_editor_translated(void* inOut)
{ EEShimOut(inOut, UWideString(EEScratch<UWideString>(kScratchCampaignEditorTranslated))); }
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

// ---- the post-game summary's records and series (post_game_calls.rs; each one statement)
#define PGR(inRecord) gWorld.mStatistics.mRecords[inRecord]
extern "C" uint32_t ee_screenscpp_pgs_records() { return (uint32_t)gWorld.mStatistics.GetRecords().size(); }
extern "C" int32_t ee_screenscpp_pgs_start_time() { return (int32_t)gWorld.mStatistics.mStartTime; }
extern "C" int32_t ee_screenscpp_pgs_end_time() { return (int32_t)gWorld.mStatistics.mEndTime; }
extern "C" int32_t ee_screenscpp_pgs_current_record() { return (int32_t)gWorld.mStatistics.mCurrentRecordIndex; }
extern "C" int32_t ee_screenscpp_pgs_tile_count() { return (int32_t)gWorld.mStatistics.mTileCount; }
extern "C" bool ee_screenscpp_pgs_victory_enabled() { return gWorld.mStatistics.mVictoryEnabled; }
extern "C" bool ee_screenscpp_pgs_is_scenario() { return gWorld.mStatistics.mIsScenario; }
extern "C" bool ee_screenscpp_pgs_is_campaign() { return gWorld.mStatistics.mIsCampaign; }
extern "C" uint8_t ee_screenscpp_pgs_current_slot() { return (uint8_t)gWorld.mStatistics.GetCurrentRecord().mSlot; }
extern "C" uint32_t ee_screenscpp_pgs_level(uint32_t inIndex)
{ return (uint32_t)EEGetResourceLevel(gGameResources)->mAmounts[inIndex]; }
extern "C" float ee_screenscpp_pr_red(uint32_t inRecord) { return PGR(inRecord).mColor.mRed; }
extern "C" float ee_screenscpp_pr_green(uint32_t inRecord) { return PGR(inRecord).mColor.mGreen; }
extern "C" float ee_screenscpp_pr_blue(uint32_t inRecord) { return PGR(inRecord).mColor.mBlue; }
extern "C" int32_t ee_screenscpp_pr_result(uint32_t inRecord) { return (int32_t)PGR(inRecord).mResult; }
extern "C" uint32_t ee_screenscpp_pr_team(uint32_t inRecord) { return (uint32_t)PGR(inRecord).mTeam; }
extern "C" uint32_t ee_screenscpp_pr_end_time(uint32_t inRecord) { return (uint32_t)PGR(inRecord).mEndTime; }
extern "C" uint32_t ee_screenscpp_pr_resource(uint32_t inRecord, uint32_t inIndex)
{ return (uint32_t)PGR(inRecord).mResources[inIndex]; }
extern "C" uint32_t ee_screenscpp_pr_score(uint32_t inRecord) { return (uint32_t)PGR(inRecord).mScore; }
extern "C" uint32_t ee_screenscpp_pr_economy_score(uint32_t inRecord) { return (uint32_t)PGR(inRecord).mEconomyScore; }
extern "C" uint32_t ee_screenscpp_pr_epoch_score(uint32_t inRecord) { return (uint32_t)PGR(inRecord).mEpochScore; }
extern "C" uint32_t ee_screenscpp_pr_military_score(uint32_t inRecord)
{ return (uint32_t)PGR(inRecord).mMilitaryScore; }
extern "C" uint32_t ee_screenscpp_pr_tech_score(uint32_t inRecord) { return (uint32_t)PGR(inRecord).mTechScore; }
extern "C" uint8_t ee_screenscpp_pr_slot(uint32_t inRecord) { return (uint8_t)PGR(inRecord).mSlot; }
extern "C" bool ee_screenscpp_pr_human(uint32_t inRecord) { return PGR(inRecord).mIsHuman; }
extern "C" uint32_t ee_screenscpp_pr_value(uint32_t inRecord, uint32_t inSeries)
{ return (uint32_t)PGR(inRecord).mStatistics.GetValue(inSeries); }
extern "C" uint32_t ee_screenscpp_pr_last_value(uint32_t inRecord, uint32_t inSeries)
{ return (uint32_t)PGR(inRecord).mStatistics.GetLastValue(inSeries); }
extern "C" uint32_t ee_screenscpp_pr_increases(uint32_t inRecord, uint32_t inSeries)
{ return (uint32_t)PGR(inRecord).mStatistics.CountIncreases(inSeries); }
extern "C" void ee_screenscpp_pr_series_load(uint32_t inRecord, uint32_t inSeries)
{ EEScratch<EEStatisticsSeries>(kScratchPgsSeries) = PGR(inRecord).mStatistics.GetSeries(inSeries); }
extern "C" uint32_t ee_screenscpp_pr_series_count() { return (uint32_t)EEScratch<EEStatisticsSeries>(kScratchPgsSeries).mPoints.size(); }
extern "C" uint32_t ee_screenscpp_pr_series_time(uint32_t inIndex)
{ return (uint32_t)EEScratch<EEStatisticsSeries>(kScratchPgsSeries).mPoints[inIndex].mTime; }
extern "C" uint32_t ee_screenscpp_pr_series_value(uint32_t inIndex)
{ return (uint32_t)EEScratch<EEStatisticsSeries>(kScratchPgsSeries).mPoints[inIndex].mValue; }
extern "C" void ee_screenscpp_pgs_graph_add_series(uintptr_t inForm, uintptr_t inPairs, uint32_t inCount, uint32_t inSeries, uint32_t inRecord)
{ static_cast<UIGraph*>(F(inForm)->GetControl(kPostGameLineGraph))->AddGraphSeries(reinterpret_cast<long*>(inPairs), inCount, inSeries, PGR(inRecord).mColor); }
extern "C" uint32_t ee_screenscpp_pgs_window_width(uintptr_t inForm)
{ return (uint32_t)F(inForm)->GetManager()->GetRasterizer()->GetWindow(F(inForm)->GetManager()->GetScreen())->GetWidth(); }
extern "C" uint32_t ee_screenscpp_pgs_window_height(uintptr_t inForm)
{ return (uint32_t)F(inForm)->GetManager()->GetRasterizer()->GetWindow(F(inForm)->GetManager()->GetScreen())->GetHeight(); }

// ---- the original dispatcher chain's cases (screens_calls.rs; each one statement)
#define MGF(inForm) static_cast<EEUIFMainGame*>(F(inForm))
static const unsigned long kScreensAxes[] = {kMouseXAxis, kMouseYAxis, kMouseMiddleButton, kMouseZAxis};
extern "C" void ee_screenscpp_sc_hk_update_selection() { gWorld.GetCurrentPlayer()->UpdateSelection(NULL, true); }
extern "C" void ee_screenscpp_sc_hk_reset_saved() { gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x14, true); }
extern "C" void ee_screenscpp_sc_hk_group_button(uintptr_t inForm, uint32_t inControl)
{ EEUIShell::Op(F(inForm)->GetControl(inControl), EE_SCREENS_SKOP_SG_SET_UNIT, (int64_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit()); }
extern "C" bool ee_screenscpp_sc_ch_mouse_inside(uintptr_t inForm)
{ return F(inForm)->GetRectangle().Contains(F(inForm)->GetMouseLocation()); }
extern "C" void ee_screenscpp_sc_cl_unpause() { gWorld.UnpauseGame(); }
extern "C" void ee_screenscpp_sc_mg_clear_gate_ghosts(uintptr_t inForm) { ee_screens_gate_clear_ghosts(&MGF(inForm)->mBuildGate); }
extern "C" void ee_screenscpp_sc_mg_set_ui_state(uintptr_t inForm, int32_t inState)
{ MGF(inForm)->SetUIState((long)inState); }
extern "C" void ee_screenscpp_sc_mg_pointer(int32_t inPointer) { gMouse->mPointer = (long)inPointer; }
extern "C" uint32_t ee_screenscpp_sc_mg_show_dialog() { return (uint32_t)gPhysics->PostShowDialogMessage(); }
extern "C" void ee_screenscpp_sc_mg_entity_group(uintptr_t inForm, uint32_t inGroup)
{ MGF(inForm)->SetEntityGroup((long)inGroup); }
extern "C" void ee_screenscpp_sc_mg_set_hidden_mode(uint32_t inMode) { gHiddenUnitsMode = (long)inMode; }
extern "C" uintptr_t ee_screenscpp_sc_units_first() { return (uintptr_t)(EEUnit*)gWorld.mUnits.mFirst; }
extern "C" uintptr_t ee_screenscpp_sc_unit_next(uintptr_t inUnit)
{ return (uintptr_t)static_cast<EEUnit*>(reinterpret_cast<EEUnit*>(inUnit)->mTileNode->mNext); }
extern "C" uintptr_t ee_screenscpp_sc_unit_complex(uintptr_t inUnit)
{ return (uintptr_t)reinterpret_cast<EEUnit*>(inUnit)->mComplexUnit; }
extern "C" bool ee_screenscpp_sc_complex_can_be_hidden(uintptr_t inComplex)
{ return reinterpret_cast<EEComplexUnit*>(inComplex)->mCanBeHidden; }
extern "C" void ee_screenscpp_sc_complex_mark_moved(uintptr_t inComplex)
{ reinterpret_cast<EEComplexUnit*>(inComplex)->mHiddenFlags |= kHiddenFlagMoved; }
extern "C" void ee_screenscpp_sc_mg_set_camera_mode(uint32_t inMode) { gWorld.mCameraControl.SetMode((long)inMode); }
extern "C" void ee_screenscpp_sc_mg_zoom_style()
{ gGamePreferences.mSettings.mZoomStyle = gWorld.mCameraControl.mMode; }
extern "C" void ee_screenscpp_sc_cam_freeze(bool inFrozen) { gMouse->mIsFrozen = inFrozen; }
extern "C" void ee_screenscpp_sc_mouse_entity_clear() { EEScratch<ISEntity*>(kScratchScreensMouse) = NULL; }
extern "C" void ee_screenscpp_sc_mouse_entity_load(uint32_t inAxis)
{ gISMouse->GetEntity(kScreensAxes[inAxis & 3], EEScratch<ISEntity*>(kScratchScreensMouse)); }
extern "C" bool ee_screenscpp_sc_mouse_entity_present() { return EEScratch<ISEntity*>(kScratchScreensMouse) != NULL; }
extern "C" int64_t ee_screenscpp_sc_mouse_entity_value() { return (int64_t)EEScratch<ISEntity*>(kScratchScreensMouse)->GetValue(); }
extern "C" int64_t ee_screenscpp_sc_mouse_entity_normalized() { return (int64_t)EEScratch<ISEntity*>(kScratchScreensMouse)->GetNormalizedValue(); }
extern "C" void ee_screenscpp_sc_cam_revolve(float inX, float inY)
{ gWorld.mCameraControl.Revolve(GE3DPoint(inX, inY, 0.0f)); }
extern "C" void ee_screenscpp_sc_cam_rotate(float inX, float inY)
{ gWorld.mCameraControl.Rotate(GE3DPoint(inX, inY, 0.0f)); }
extern "C" void ee_screenscpp_sc_cam_scroll(float inX, float inY) { gWorld.mCameraControl.Scroll(inX, inY); }
extern "C" void ee_screenscpp_sc_cam_reset_zoom() { gWorld.mCameraControl.ResetZoom(); }
extern "C" void ee_screenscpp_sc_cam_changed() { gWorld.mCameraControl.SetChanged(); }
extern "C" void ee_screenscpp_sc_cam_set_zoom(float inZoom) { gWorld.mCameraControl.SetZoom(inZoom); }
extern "C" float ee_screenscpp_sc_cam_cos(float inAngle) { return UMCos(inAngle); }
extern "C" uint32_t ee_screenscpp_sc_ds_button_number(uintptr_t inForm, uint32_t inControl)
{ return (uint32_t)atoi(static_cast<UIButton*>(F(inForm)->GetControl(inControl))->GetGEText()->GetText()); }
extern "C" uint32_t ee_screenscpp_sc_ds_label_number(uintptr_t inForm, uint32_t inControl)
{ return (uint32_t)static_cast<UITextLabel*>(F(inForm)->GetControl(inControl))->GetTextAsInt(0); }
extern "C" void ee_screenscpp_sc_pgs_set_page(int32_t inPage)
{ gGamePreferences.mSettings.mLastOpenPostGameStatsPage = (long)inPage; }
extern "C" void ee_screenscpp_sc_pgs_set_r(uint32_t inIndex, float inValue) { EEScratch<float[4]>(kScratchPgsRectangle)[inIndex & 3] = inValue; }
extern "C" void ee_screenscpp_sc_pgs_apply_r(uintptr_t inForm, uint32_t inControl)
{ F(inForm)->GetControl(inControl)->SetRectangle(GERectangle(EEScratch<float[4]>(kScratchPgsRectangle)[0], EEScratch<float[4]>(kScratchPgsRectangle)[1], EEScratch<float[4]>(kScratchPgsRectangle)[2], EEScratch<float[4]>(kScratchPgsRectangle)[3])); }
extern "C" void ee_screenscpp_sc_pgs_new_manager(uintptr_t inForm) { F(inForm)->SetFormManager(new EEPGSFormManager); }
extern "C" bool ee_screenscpp_sc_pgs_is_bitmap(uintptr_t inForm, uint32_t inControl)
{ return dynamic_cast<UIBitmap*>(F(inForm)->GetControl(inControl)) != NULL; }
extern "C" void ee_screenscpp_sc_pgs_protocol() { NEProtocol::GetProtocol(0, EEScratch<NEProtocol*>(kScratchPgsProtocol)); }
extern "C" int32_t ee_screenscpp_sc_preferences_save() { return (int32_t)gGamePreferences.Save(); }
extern "C" void ee_screenscpp_sc_pdcs_fill(uintptr_t inForm, uint32_t inControl)
{ gCivilizationManager.FillCivilizationList(static_cast<EECBUIListBox*>(F(inForm)->GetControl(inControl))); }
extern "C" uintptr_t ee_screenscpp_sc_pdcs_control(uint32_t inIndex)
{ return (uintptr_t)EEScreensPdcsControl(inIndex); }
extern "C" void ee_screenscpp_sc_pdcs_clear_stats() { EEScreensPdcsControl(4)->DeleteAllEntries(); }
extern "C" void ee_screenscpp_sc_pdcs_set_upgrades()
{ EEScreensPdcsControl(4)->SetCivilizationUpgrades(EEScratch<std::vector<EECivUpgrade>>(kScratchPdcsUpgrades)); }
extern "C" void ee_screenscpp_sc_pdcs_hide_label() { EEScreensPdcsControl(5)->SetDisplayMode(kDisplayModeHidden); }
extern "C" void ee_screenscpp_sc_pdcs_reset() { EEScreensPdcsControl(3)->ResetSelection(); }
extern "C" void ee_screenscpp_sc_dsu_clear() { EEScratch<EESoundObject*>(kScratchDebugSound) = NULL; }
extern "C" void ee_screenscpp_sc_dsu_unit_clear() { EEScratch<EEComplexUnit*>(kScratchDsuUnit) = NULL; }
extern "C" bool ee_screenscpp_sc_dsu_first_selected() { return EEGetFirstSelectedUnit(EEScratch<EEComplexUnit*>(kScratchDsuUnit)); }
extern "C" bool ee_screenscpp_sc_dsu_has_complex() { return EEScratch<EEComplexUnit*>(kScratchDsuUnit)->mComplexUnit != NULL; }
extern "C" void ee_screenscpp_sc_dsu_sound1() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mAcknowledgeSounds[0]; }
extern "C" void ee_screenscpp_sc_dsu_sound2() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mAcknowledgeSounds[1]; }
extern "C" void ee_screenscpp_sc_dsu_sound3() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mCreationSound; }
extern "C" void ee_screenscpp_sc_dsu_sound4() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mDeathSound; }
extern "C" void ee_screenscpp_sc_dsu_sound5() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mSelectionSounds[0]; }
extern "C" void ee_screenscpp_sc_dsu_sound6() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mSelectionSounds[1]; }
extern "C" void ee_screenscpp_sc_dsu_sound7() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mModel->mSounds1[0]; }
extern "C" void ee_screenscpp_sc_dsu_sound8() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mModel->mSounds2[0]; }
extern "C" void ee_screenscpp_sc_dsu_sound9() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mModel->mSounds1[13]; }
extern "C" void ee_screenscpp_sc_dsu_sound10() { EEScratch<EESoundObject*>(kScratchDebugSound) = DSUC()->mModel->mSounds2[13]; }
extern "C" bool ee_screenscpp_sc_dsu_has_sound() { return EEScratch<EESoundObject*>(kScratchDebugSound) != NULL; }
extern "C" void ee_screenscpp_sc_dsu_volume(float inVolume) { EEScratch<EESoundObject*>(kScratchDebugSound)->SetVolume(inVolume); }
extern "C" bool ee_screenscpp_sc_ce_is_edit(uintptr_t inForm, uint32_t inControl)
{ return dynamic_cast<UIEditBox*>(F(inForm)->GetControl(inControl)) != NULL; }
extern "C" bool ee_screenscpp_sc_ce_is_label(uintptr_t inForm, uint32_t inControl)
{ return dynamic_cast<UITextLabel*>(F(inForm)->GetControl(inControl)) != NULL; }
extern "C" bool ee_screenscpp_sc_ce_is_list(uintptr_t inForm, uint32_t inControl)
{ return dynamic_cast<UIListBox<UWideString>*>(F(inForm)->GetControl(inControl)) != NULL; }
extern "C" bool ee_screenscpp_sc_ce_is_combo(uintptr_t inForm, uint32_t inControl)
{ return dynamic_cast<UIFileComboBox*>(F(inForm)->GetControl(inControl)) != NULL; }
extern "C" void ee_screenscpp_sc_won_net_clear() { gWonNet = NULL; }
extern "C" void ee_screenscpp_sc_won_net_delete(uintptr_t inWon) { delete reinterpret_cast<EEWonNet*>(inWon); }
extern "C" void ee_screenscpp_sc_send_player_information(uint32_t inA, uint32_t inB, int32_t inValue)
{ gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessagePlayerInformation, {(long)inValue, (long)inA, 0.0f, (long)inB}), kSlotNone); }
extern "C" void ee_screenscpp_sc_send_game_option(int32_t inOption, int32_t inValue)
{ gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageGameOptions, {(long)inOption, (long)inValue, UWideString("")}), kSlotNone); }
extern "C" void ee_screenscpp_sc_send_start_loading(uint8_t inFlag, int32_t inTime)
{ gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageStartLoading, {(unsigned char)inFlag, (int32_t)inTime}), kSlotNone); }
extern "C" void ee_screenscpp_sc_game_state_multiplayer(bool inMultiplayer)
{ gGameState.mGameOptions.mIsMultiplayer = inMultiplayer; }

// ---- the original ee_screenscpp_text's and _put_text's cases (texts_calls.rs; each one statement) and their
// locals
extern "C" void ee_screenscpp_tp_player(EeScreensShimText inText) { gPlayerName = EEShimText(inText); }
extern "C" void ee_screenscpp_tp_rasterizer_setting(EeScreensShimText inText)
{ gGamePreferences.GetSettings().mRasterizerName = std::string((const char*)inText.ptr, inText.len).c_str(); }
extern "C" void ee_screenscpp_tp_audio_device(EeScreensShimText inText)
{ snprintf(gGamePreferences.GetSettings().mAudioDevice, sizeof(gGamePreferences.GetSettings().mAudioDevice), "%s", std::string((const char*)inText.ptr, inText.len).c_str()); }
extern "C" uint32_t ee_screenscpp_tp_campaign_built_in(EeScreensShimText inText)
{ return (uint32_t)EECampaignFormManager::GetBuiltInCampaign(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_message_none_found(EeScreensShimText inText)
{ gUIManager->DisplayMessage(EEShimText(inText), true, NULL, EEScreensNoCustomCampaigns, true); }
extern "C" void ee_screenscpp_tp_message_close_campaign(EeScreensShimText inText)
{ gUIManager->DisplayMessage(EEShimText(inText), true, NULL, EEScreensCloseCampaign, true); }
extern "C" void ee_screenscpp_tp_cs_scenario_name(EeScreensShimText inText)
{ EECampaignFormManager::Get()->mScenarioName = EEShimText(inText); }
extern "C" void ee_screenscpp_tp_question(uintptr_t inForm, EeScreensShimText inText)
{ gUIManager->DisplayQuestion(EEShimText(inText), true, F(inForm), EEScreensRestoreDefaultYes, NULL); }
extern "C" void ee_screenscpp_tp_message(EeScreensShimText inText, bool inClose)
{ gUIManager->DisplayMessage(EEShimText(inText), true, NULL, 0, inClose); }
extern "C" void ee_screenscpp_tp_mg_chat_log(EeScreensShimText inText)
{ gUserInterface->AddChatLogEntry(EEShimText(inText), 0); }
extern "C" void ee_screenscpp_tp_ch_chat(EeScreensShimText inText)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageInGameChat, {EEShimText(inText)}), kSlotNone); }
extern "C" void ee_screenscpp_tp_cl_append(EeScreensShimText inText)
{ static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox))->AppendText(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_campaign_name(EeScreensShimText inText)
{ EECampaignFormManager::Get()->SetCampaignName(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_campaign_description(EeScreensShimText inText)
{ EECampaignFormManager::Get()->SetDescription(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_campaign_first(EeScreensShimText inText)
{ EECampaignFormManager::Get()->SetFirstScenario(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_load_saved_name(EeScreensShimText inText)
{ gGamePreferences.mSettings.SetLoadSavedGameName(EEShimString(inText)); }
extern "C" void ee_screenscpp_tp_load_scenario_name(EeScreensShimText inText)
{ gGamePreferences.mSettings.SetLoadScenarioName(EEShimString(inText)); }
// the questions' answers (question_answers_ffi.rs) and the C++ they call
static EEUIFErrorCallback EEScreensAnswer(uint32_t inTable, uint32_t inIndex)
{ return reinterpret_cast<EEUIFErrorCallback>(const_cast<void*>(ee_screens_question_answer(inTable, inIndex))); }
extern "C" void ee_screenscpp_save_send_wait(UIForm* inMessage)
{ static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(reinterpret_cast<NEMessage*>(inMessage), 15000, 12); }
extern "C" void ee_screenscpp_save_post(UIForm* inAction) { gUserInterface->PostAction(reinterpret_cast<EEAction*>(inAction)); }
extern "C" void ee_screenscpp_save_release(UIForm* inAction) { reinterpret_cast<EEAction*>(inAction)->Dereference(); }
extern "C" void ee_screenscpp_lobby_boot_set(UIForm* inCopy) { EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot) = reinterpret_cast<EEGameOptionsPlayer*>(inCopy); }
extern "C" void ee_screenscpp_tp_question_delete(uintptr_t inForm, uint32_t inControl, EeScreensShimText inText)
{ gUIManager->DisplayQuestion(EEShimText(inText), true, reinterpret_cast<UIForm*>(static_cast<UIFileListBox*>(F(inForm)->GetControl(inControl))), EEScreensDeleteSelectedFile, 0); }
extern "C" void ee_screenscpp_tp_save_message(EeScreensShimText inText)
{ EEScratch<UIForm*>(kScratchSaveObject) = reinterpret_cast<UIForm*>(static_cast<NEMessage*>(new EEMRecord(kMessageLoadOrSaveGame, {0, 0, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 0, 0, EEShimText(inText)}))); }
extern "C" void ee_screenscpp_tp_save_action(EeScreensShimText inName, EeScreensShimText inDescription)
{ EEScratch<UIForm*>(kScratchSaveObject) = reinterpret_cast<UIForm*>(static_cast<EEAction*>(new EEASaveGame(EEShimString(inName), true, EEShimString(inDescription)))); }
extern "C" void ee_screenscpp_tp_save_question(uint32_t inPost, EeScreensShimText inText)
{ gUIManager->DisplayQuestion(EEShimText(inText), true, EEScratch<UIForm*>(kScratchSaveObject), EEScreensAnswer(EE_SCREENS_QA_SAVE_YES, inPost), EEScreensDropSave); }
extern "C" void ee_screenscpp_tp_le_map_type(EeScreensShimText inText) { gGameOptions.mMapType = EEShimText(inText); }
extern "C" void ee_screenscpp_tp_le_send_map_type(uint32_t inValue, EeScreensShimText inText)
{ gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageGameOptions, {2, (long)inValue, EEShimText(inText)}), kSlotNone); }
extern "C" void ee_screenscpp_tp_le_chat(EeScreensShimText inText)
{ gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageChat, {0, EEShimText(inText)}), kSlotNone); }
extern "C" void ee_screenscpp_tp_le_boot_question(EeScreensShimText inText)
{ gUIManager->DisplayQuestion(EEShimText(inText), true, reinterpret_cast<UIForm*>(EEScratch<EEGameOptionsPlayer*>(kScratchLobbyBoot)), EEScreensAnswer(EE_SCREENS_QA_BOOT, 0), EEScreensAnswer(EE_SCREENS_QA_BOOT, 1)); }
extern "C" uintptr_t ee_screenscpp_tp_ui_manager() { return (uintptr_t)static_cast<UIManager*>(gUIManager); }
extern "C" void ee_screenscpp_tp_go_question(uintptr_t inManager, uint32_t inYes, EeScreensShimText inText)
{ static_cast<EEUIManager*>(reinterpret_cast<UIManager*>(inManager))->DisplayQuestion(EEShimText(inText), true, NULL, EEScreensAnswer(EE_SCREENS_QA_GO_YES, inYes), 0); }
extern "C" void ee_screenscpp_tp_pgs_bar(uintptr_t inForm, uint32_t inControl, EeScreensShimText inText)
{ static_cast<UIStatusBar*>(F(inForm)->GetControl(inControl))->SetText(EEShimText(inText), NULL); }
extern "C" void ee_screenscpp_tp_csel_form() { gUIManager->GetFormByID(kFormCampaignScenarioSelect); }
extern "C" void ee_screenscpp_tp_pgs_campaign_get() { EEScratch<EECampaignFormManager*>(kScratchPgsCampaign) = EECampaignFormManager::Get(); }
extern "C" void ee_screenscpp_tp_pgs_file_name(EeScreensShimText inText)
{ EEScratch<UWideString>(kScratchPgsFileName) = "Campaigns\\" + EEShimText(inText) + gCampaignExtension; }
extern "C" void ee_screenscpp_tp_pgs_error_clear() { EEScratch<UWideString>(kScratchPgsError) = UWideString(); }
extern "C" uint32_t ee_screenscpp_tp_pgs_campaign_load()
{ return (uint32_t)EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->mCampaign.Load(EEScratch<UWideString>(kScratchPgsFileName), EEScratch<UWideString>(kScratchPgsError), false); }
extern "C" void ee_screenscpp_tp_pgs_campaign_name(EeScreensShimText inText)
{ EEScratch<EECampaignFormManager*>(kScratchPgsCampaign)->SetCampaignName(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_pdcs_label(EeScreensShimText inText)
{ static_cast<UITextLabel*>(static_cast<UIControl*>(EEScreensPdcsControl(5)))->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_ce_question(uintptr_t inManager, uintptr_t inForm, uint32_t inIndex, EeScreensShimText inText)
{ static_cast<EEUIManager*>(reinterpret_cast<UIManager*>(inManager))->DisplayQuestion(EEShimText(inText), true, F(inForm), EEScreensAnswer(EE_SCREENS_QA_CE_YES, inIndex), EEScreensAnswer(EE_SCREENS_QA_CE_NO, inIndex)); }
extern "C" void ee_screenscpp_tp_ce_lines_clear() { EEScratch<std::vector<UWideString>>(kScratchCELines).clear(); }
extern "C" void ee_screenscpp_tp_ce_lines_add(EeScreensShimText inText) { EEScratch<std::vector<UWideString>>(kScratchCELines).push_back(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_ce_lines_append(uintptr_t inForm, uint32_t inControl)
{ static_cast<UIListBox<UWideString>*>(F(inForm)->GetControl(inControl))->AppendStringArray(EEScratch<std::vector<UWideString>>(kScratchCELines)); }
extern "C" bool ee_screenscpp_tp_ce_bg_equals(uintptr_t inForm, uint32_t inIndex, EeScreensShimText inText)
{ return static_cast<UIFileComboBox*>(F(inForm)->GetControl(kCampaignEditorBackgroundImageComboBox))->GetListBox()->GetData(inIndex).EqualsIgnoreCase(EEShimText(inText)); }
extern "C" void ee_screenscpp_tp_ce_translate(EeScreensShimText inText)
{ EETranslateString(EEShimText(inText), EEScratch<UWideString>(kScratchCampaignEditorTranslated)); }
extern "C" uint32_t ee_screenscpp_tt_pdcs_civ_name_id(uint32_t inIndex) { return (uint32_t)EEScratch<std::vector<EECivilizationEntry>>(kScratchPdcsCivs)[inIndex].mNameID; }
extern "C" void ee_screenscpp_tt_pdcs_civ_file(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, UWideString(EEScratch<std::vector<EECivilizationEntry>>(kScratchPdcsCivs)[inIndex].GetFileName())); }
extern "C" void ee_screenscpp_tt_spec_installed_directory(uintptr_t inSpec)
{ S(inSpec)->SetDirectory(gGamePreferences.GetSettings().mInstalledFromDirectory); }
extern "C" int32_t ee_screenscpp_tt_spec_fopen_read(uintptr_t inSpec) { return (int32_t)S(inSpec)->FOpen("r"); }
extern "C" void ee_screenscpp_tt_spec_scan_server(uintptr_t inSpec)
{ fscanf(*S(inSpec), "%s%s", EEScratch<char[32]>(kScratchServerHost), EEScratch<char[20]>(kScratchServerPort)); }
extern "C" void ee_screenscpp_tt_server_host(void* inOut) { EEShimOutC(inOut, EEScratch<char[32]>(kScratchServerHost)); }
extern "C" void ee_screenscpp_tt_server_port(void* inOut) { EEShimOutC(inOut, EEScratch<char[20]>(kScratchServerPort)); }
extern "C" void ee_screenscpp_tt_cf_files_clear() { EEScratch<std::set<UString>>(kScratchTextFiles).clear(); }
extern "C" void ee_screenscpp_tt_cf_path_new() { EEScratch<FSPath*>(kScratchTextPath) = new FSPath; }
extern "C" void ee_screenscpp_tt_cf_directory_reset() { EEScratch<FSFileSpec>(kScratchTextDirectory) = FSFileSpec(); }
extern "C" void ee_screenscpp_tt_cf_directory_volume() { EEScratch<FSFileSpec>(kScratchTextDirectory).SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_tt_cf_directory_campaigns()
{ EEScratch<FSFileSpec>(kScratchTextDirectory).SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\"); }
extern "C" void ee_screenscpp_tt_cf_path_add() { EEScratch<FSPath*>(kScratchTextPath)->AddDirectory(EEScratch<FSFileSpec>(kScratchTextDirectory), true); }
extern "C" void ee_screenscpp_tt_cf_list_campaigns()
{ EEScratch<FSPath*>(kScratchTextPath)->ListFiles(UString("*") + gCampaignExtension, EEScratch<std::set<UString>>(kScratchTextFiles)); }
extern "C" void ee_screenscpp_tt_cf_path_dereference() { EEScratch<FSPath*>(kScratchTextPath)->Dereference(); }
extern "C" void ee_screenscpp_tt_cf_snapshot() { EEScratch<std::vector<UString>>(kScratchTextLines).assign(EEScratch<std::set<UString>>(kScratchTextFiles).begin(), EEScratch<std::set<UString>>(kScratchTextFiles).end()); }
extern "C" uint32_t ee_screenscpp_tt_lines_count() { return (uint32_t)EEScratch<std::vector<UString>>(kScratchTextLines).size(); }
extern "C" void ee_screenscpp_tt_line(uint32_t inIndex, void* inOut) { EEShimOut(inOut, EEScratch<std::vector<UString>>(kScratchTextLines)[inIndex]); }
extern "C" void ee_screenscpp_tt_list_clear() { EEScratch<std::vector<UWideString>>(kScratchTextList).clear(); }
extern "C" void ee_screenscpp_tt_adapters_protocol_clear() { EEScratch<NEProtocol*>(kScratchAdaptersProtocol) = NULL; }
extern "C" void ee_screenscpp_tt_adapters_protocol() { NEProtocol::GetProtocol(0, EEScratch<NEProtocol*>(kScratchAdaptersProtocol)); }
extern "C" bool ee_screenscpp_tt_adapters_has_protocol() { return EEScratch<NEProtocol*>(kScratchAdaptersProtocol) != NULL; }
extern "C" void ee_screenscpp_tt_adapters_enumerate() { EEScratch<NEProtocol*>(kScratchAdaptersProtocol)->EnumerateNetworkAdapters(EEScratch<std::vector<UWideString>>(kScratchTextList)); }
extern "C" uint32_t ee_screenscpp_tt_list_count() { return (uint32_t)EEScratch<std::vector<UWideString>>(kScratchTextList).size(); }
extern "C" void ee_screenscpp_tt_list_item(uint32_t inIndex, void* inOut) { EEShimOut(inOut, EEScratch<std::vector<UWideString>>(kScratchTextList)[inIndex]); }
extern "C" uint32_t ee_screenscpp_tt_output_devices() { return (uint32_t)SPIMGetOutputDevices(EEScratch<char[16][128]>(kScratchOutputNames), 16); }
extern "C" void ee_screenscpp_tt_output_device(uint32_t inIndex, void* inOut)
{ EEShimOutC(inOut, EEScratch<char[16][128]>(kScratchOutputNames)[inIndex]); }
extern "C" void ee_screenscpp_tt_cs_scenarios() { EEScratch<std::vector<UWideString>>(kScratchTextList) = EECampaignFormManager::Get()->mCampaign.GetScenarios(); }
extern "C" void ee_screenscpp_tt_cs_progress_new() { EEScratch<EECampaignProgress*>(kScratchTextProgress) = new EECampaignProgress; }
// the campaign progress of the user "default" (0x5d2ae0 gCampaignDefaultUser)
extern "C" int64_t ee_screenscpp_tt_cs_progress_load()
{ return (int64_t)EEScratch<EECampaignProgress*>(kScratchTextProgress)->Load(UWideString("default"), EECampaignFormManager::Get()->mCampaignName, EECampaignFormManager::Get()->GetChecksum(), false); }
extern "C" void ee_screenscpp_tt_cs_progress_completed() { EEScratch<EECampaignProgress*>(kScratchTextProgress)->GetCompletedScenarios(EEScratch<std::vector<UWideString>>(kScratchTextList)); }
extern "C" void ee_screenscpp_tt_cs_progress_delete() { delete EEScratch<EECampaignProgress*>(kScratchTextProgress); }
extern "C" void ee_screenscpp_tt_cs_progress_format()
{ gResource->GetString(IDS_CAMPAIGN_IN_PROGRESS_OF_COMPLETED, EEScratch<UWideString>(kScratchTextFormat)); }
extern "C" void ee_screenscpp_tt_cs_progress_print(uint32_t inA, uint32_t inB)
{ sprintf(EEScratch<UWideString>(kScratchTextOut), EEScratch<UWideString>(kScratchTextFormat), (size_t)inA, (size_t)inB); }
extern "C" void ee_screenscpp_tt_text_out(void* inOut) { EEShimOut(inOut, EEScratch<UWideString>(kScratchTextOut)); }
extern "C" void ee_screenscpp_tt_generator_display(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, UWideString((const char*)EERMGetGeneratorDisplayName((unsigned long)inIndex))); }
extern "C" void ee_screenscpp_tt_generator_name(uint32_t inIndex, void* inOut)
{ EEShimOut(inOut, UWideString(EERMGetGeneratorName(inIndex))); }
extern "C" void ee_screenscpp_tt_engine_ai_name(uintptr_t inAi, void* inOut)
{ EEShimOut(inOut, UWideString(EECPAIText(reinterpret_cast<const EECPAIEntry*>(inAi)->name, reinterpret_cast<const EECPAIEntry*>(inAi)->name_len).c_str())); }

// ---- the original ee_screenscpp_call's own cases (screens_calls.rs root; each one statement)
extern "C" void ee_screenscpp_rc_resize_menu()
{ gUserInterface->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false); }
extern "C" void ee_screenscpp_rc_srand_time() { srand(TMGetTime()); }
extern "C" uint32_t ee_screenscpp_rc_rand() { return (uint32_t)rand(); }
extern "C" void ee_screenscpp_rc_ui_music_volume(float inVolume) { gUIManager->SetMusicVolume(inVolume); }
extern "C" void ee_screenscpp_rc_music_start() { gMusicManager->Start(); }
extern "C" void ee_screenscpp_rc_music_stop() { gMusicManager->Stop(); }
extern "C" void ee_screenscpp_rc_mouse_sensitivity(float inValue) { gMouse->SetSensitivity(inValue); }
extern "C" void ee_screenscpp_rc_apply_port() { EEApplyPortSettings(gGamePreferences.GetSettings()); }
extern "C" void ee_screenscpp_rc_mip_filter(uint32_t inMode) { gTextureManager->UpdateMIPFilterMode(inMode); }
extern "C" void ee_screenscpp_rc_texture_resolution(int32_t inValue) { EESetTextureResolution(inValue); }
extern "C" void ee_screenscpp_rc_ambient(bool inEnabled) { gAmbientSoundManager.SetEnabled(inEnabled); }
extern "C" uint32_t ee_screenscpp_rc_display_rasterizers() { return (uint32_t)gDisplayRasterizers.size(); }
extern "C" uint32_t ee_screenscpp_rc_display_screens(uint32_t inRasterizer)
{ return (uint32_t)gDisplayRasterizers[inRasterizer].mScreens.size(); }
extern "C" void ee_screenscpp_rc_external_lobby() { EEStartExternalLobby(); }
extern "C" void ee_screenscpp_rc_start_editor(uintptr_t inManager)
{ static_cast<EEUIManager*>(reinterpret_cast<UIManager*>(inManager))->StartScenarioEditor(); }
extern "C" void ee_screenscpp_rc_forget_main_form() { gSavedMainGameForm = NULL; }
extern "C" void ee_screenscpp_rc_show_single_player() { EEShowSinglePlayerScreen(); }
extern "C" void ee_screenscpp_rc_campaign_all_open(bool inOpen)
{ EECampaignFormManager::Get()->mAllScenariosOpen = inOpen; }
extern "C" void ee_screenscpp_rc_save_send() { ee_screens_save_send(EEScratch<UIForm*>(kScratchSaveObject)); }
extern "C" void ee_screenscpp_rc_save_post() { ee_screens_save_post(EEScratch<UIForm*>(kScratchSaveObject)); }
extern "C" void ee_screenscpp_rc_open_saved_main_form() { gUserInterface->OpenForm(gSavedMainGameForm); }
extern "C" uint32_t ee_screenscpp_rc_si_get_scenario()
{ return (uint32_t)EEUISCIFormManager::Get()->GetScenario(EEScreensScenario()); }
extern "C" void ee_screenscpp_rc_si_post_load() { gUserInterface->PostLoadSavedGame(EEScreensScenario()); }
extern "C" void ee_screenscpp_rc_campaign_update_checksum() { EECampaignFormManager::Get()->UpdateChecksum(); }
extern "C" void ee_screenscpp_rc_le_bitmap_on(uintptr_t inForm, uint32_t inControl)
{ static_cast<UIBitmap*>(F(inForm)->GetControl(inControl))->SetBitmap(gUIManager->GetCheckDownOn()); }
extern "C" void ee_screenscpp_rc_le_bitmap_off(uintptr_t inForm, uint32_t inControl)
{ static_cast<UIBitmap*>(F(inForm)->GetControl(inControl))->SetBitmap(gUIManager->GetCheckDownOff()); }
extern "C" void ee_screenscpp_rc_le_ping(uintptr_t inForm, uint32_t inControl, float inPing)
{ static_cast<EEUICPing*>(F(inForm)->GetControl(inControl))->SetPing(inPing); }
extern "C" void ee_screenscpp_rc_le_disconnect()
{ gCommunications->ProcessGuaranteedLocalUserInput(new NEMRecord(kNetMessageClientDisconnectionNotification, {false}), kSlotNone); }
extern "C" void ee_screenscpp_rc_le_save_last_options() { gGamePreferences.SetLastGameOptions(gGameOptions); }
extern "C" void ee_screenscpp_rc_le_start_ping() { static_cast<EEServer*>(gCommunications)->StartPing(); }
extern "C" uint32_t ee_screenscpp_rc_le_rule(uint32_t inWhich, uint32_t inArg)
{ return EEScreensLobbyRule(inWhich, inArg); }
extern "C" void ee_screenscpp_rc_go_request_pause()
{ gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientRequestPause), kSlotNone); }
extern "C" void ee_screenscpp_rc_go_briefing() { EEDisplayScenarioIntro(); }
extern "C" void ee_screenscpp_rc_go_exit() { EEExitGame(); }

// TEST ONLY: the world lock, for the SGVW trace's note of the reading thread's holds (ee_game screens/sgvw.rs)
extern "C" const void* ee_screenscpp_sgvw_world_lock() { return gWorld.mLock; }
