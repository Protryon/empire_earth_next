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
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIControlsTable.h"
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
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGameSound.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include <locale.h>
#include "ee/EECPEngine.h"
#include "ee/EEGamePreferences.h"
#include "ee/EECBUIListBox.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUIEWonNetEvent.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIBGenericBackground.h"


#include "lle/UIBackgroundTable.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEUICPortrait.h"
#include "ee/EEUICPing.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEUICWonNetStatBox.h"
#include "lle/UIGraph.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreensShims.h"
#include "ee/EEGameState.h"   // gGameState

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
    ee_screens_ui_manager_new(this, EEUIManagerCall(this).Ptr());
}

// Destroys the UI manager and its scenario editor form
EEUIManager::~EEUIManager()
{
    delete this->GetScenarioEditorForm();
    ee_screens_ui_manager_fields_drop(this);
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
    return reinterpret_cast<UIEvent*>(ee_screens_um_game_event((uintptr_t)inForm, (uint32_t)inID));   // (ui_manager_calls.rs)
}

// Creates the event a form fires when it opens, closes or activates; NULL for other ids (ee_screens factories.rs;
// WON's server select open is C++)
UIEvent* EECreateFormEvent(UIManager* inManager, unsigned long inID) { return reinterpret_cast<UIEvent*>(ee_screens_um_form_event((uint32_t)inID)); }

// Creates the WON server select form's cancel event; NULL for other ids
UIEvent* EECreateWonNetServerSelectEvent(UIForm* inForm, unsigned long inID) { return reinterpret_cast<UIEvent*>(ee_screens_um_wns_event((uint32_t)inID)); }




// (the ids' agreement with ee_screens ui_manager.rs' is ui_ids.rs' test)

// The Rust's calls on the manager, the toolkit, the world and the threads (EE_SCREENS_UM_OP_*, ui_manager_ffi.rs
// picks), each the original's
extern "C" int64_t ee_screenscpp_um_engine_count() { return EECPEngineChoiceCount(); }
extern "C" void ee_screenscpp_um_music(void* inCall)
{ static_cast<EEUIManager*>(static_cast<EEUIManagerCall*>(inCall)->mManager)->SetMusicVolume(!gGamePreferences.mSettings.mMusicEnabled ? 0.0f : gGamePreferences.mSettings.mMusicVolume); }
extern "C" int64_t ee_screenscpp_um_citizens() { return gWorldTable.GetLongValue(kWorldStartingCitizensDefault); }
extern "C" int64_t ee_screenscpp_um_base_keyboard(void* inCall)
{ return EEUIManager::BaseKeyboard(static_cast<EEUIManager*>(static_cast<EEUIManagerCall*>(inCall)->mManager)); }
extern "C" int64_t ee_screenscpp_um_base_mouse(void* inCall)
{ return EEUIManager::BaseMouse(static_cast<EEUIManager*>(static_cast<EEUIManagerCall*>(inCall)->mManager), *static_cast<const GE2DPoint*>(static_cast<EEUIManagerCall*>(inCall)->mMouse)); }

// ---- the one-statement shims of ee_screens ui_manager_calls.rs (UI lane round 8): each one call, no logic
#define UMCALL(x) (*reinterpret_cast<EEUIManagerCall*>(x))
#define UMG(x) reinterpret_cast<EEUIManager*>(x)
#define UMF(x) reinterpret_cast<UIForm*>(x)
#define UMCT(x) (*reinterpret_cast<UIControlsTable*>(x))
#define UMBT(x) (*reinterpret_cast<const UIBackgroundTable*>(x))
#define UMCTL(x) (*reinterpret_cast<const EeScreensManagerControl*>(x))
#define UMMOVE(x) (*reinterpret_cast<const EeScreensManagerMove*>(x))
static const unsigned long kUMKeys[] = {kKeyEscape, kKeyReturn, kKeySpace, kKeyF9};
// (ui_manager_calls.rs' ids equal the headers': hud_cpp_consts_tests.rs)
extern "C" uintptr_t ee_screenscpp_um_call_manager(uintptr_t inCall) { return (uintptr_t)static_cast<EEUIManager*>(UMCALL(inCall).mManager); }
extern "C" void ee_screenscpp_um_register_game_event(uintptr_t inManager)
{ UMG(inManager)->RegisterControlEventCallback(EECreateGameEvent); }
extern "C" void ee_screenscpp_um_register_wns_event(uintptr_t inManager)
{ UMG(inManager)->RegisterControlEventCallback(EECreateWonNetServerSelectEvent); }
extern "C" void ee_screenscpp_um_register_form_event(uintptr_t inManager)
{ UMG(inManager)->RegisterFormEventCallback(EECreateFormEvent); }
extern "C" void ee_screenscpp_um_load_database(uintptr_t inManager)
{ UMG(inManager)->LoadDatabase(gDataPath, UString()); }
extern "C" int64_t ee_screenscpp_um_add_like(uintptr_t inManager, uintptr_t inControl)
{ return UIControlsTable::AddControlLike(UMG(inManager), UMCTL(inControl).template_id, UMCTL(inControl).id, UMCTL(inControl).form_index, UMCTL(inControl).group, UMCTL(inControl).left, UMCTL(inControl).right, UMCTL(inControl).top, UMCTL(inControl).bottom, UMCTL(inControl).string_begin, UMCTL(inControl).string_end, UMCTL(inControl).help); }
extern "C" int64_t ee_screenscpp_um_move(uintptr_t inMoves)
{ return UIControlsTable::MoveControlsHorizontally(UMMOVE(inMoves).form, UMMOVE(inMoves).first, UMMOVE(inMoves).last, UMMOVE(inMoves).left_delta, UMMOVE(inMoves).right_delta); }
extern "C" void ee_screenscpp_um_clear_u138(uintptr_t inManager) { UMG(inManager)->mU138 = 0; }
extern "C" void ee_screenscpp_um_save_form_set_activate(uintptr_t inManager, uintptr_t inEvent)
{ UMG(inManager)->GetFormByID(kFormSaveGame)->SetFormActivateEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_um_set_in_editor(uintptr_t inManager) { UMG(inManager)->mInScenarioEditor = true; }
extern "C" void ee_screenscpp_um_clear_multiplayer() { gGameState.mGameOptions.mIsMultiplayer = false; }   // (the original's gGameStateIsMultiplayer)
extern "C" bool ee_screenscpp_comms_present() { return gCommunications != NULL; }
extern "C" void ee_screenscpp_comms_delete() { delete gCommunications; }
extern "C" void ee_screenscpp_um_new_server()
{ gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true); }
extern "C" void ee_screenscpp_server_clear_load_responses()
{ static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0; }
extern "C" void ee_screenscpp_lobby_set_map_size(uint32_t inValue) { gWorld.GetLobbyOptions().mMapSize = inValue; }
extern "C" void ee_screenscpp_lobby_set_load_type(uint32_t inValue) { gWorld.GetLobbyOptions().mLoadType = inValue; }
extern "C" void ee_screenscpp_lobby_set_epoch_prehistoric()
{ gWorld.GetLobbyOptions().mEpochStart = kTechEpochPrehistoric; }
extern "C" void ee_screenscpp_lobby_set_variant(uint32_t inValue) { gWorld.GetLobbyOptions().mVariant = inValue; }
extern "C" void ee_screenscpp_um_name_human(uintptr_t inCall)
{ gResource->GetString(IDS_SE_HUMAN, UMCALL(inCall).mNames[0]); }
extern "C" void ee_screenscpp_um_name_computer(uintptr_t inCall)
{ gResource->GetString(IDS_COMPUTER, UMCALL(inCall).mNames[1]); }
extern "C" void ee_screenscpp_lobby_player_reset(uint32_t inSlot)
{ gWorld.GetLobbyOptions().mPlayers[inSlot] = EEGameOptionsPlayer(); }
extern "C" void ee_screenscpp_lobby_player_set_type(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mType = inValue; }
extern "C" void ee_screenscpp_um_lobby_player_set_name(uintptr_t inCall, uint32_t inSlot, uint32_t inName)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mName = UMCALL(inCall).mNames[inName]; }
extern "C" void ee_screenscpp_lobby_player_set_slot(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mSlot = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_color(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mColor = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_citizens(uint32_t inSlot, uint32_t inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mStartingCitizens = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_local(uint32_t inSlot, bool inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mIsLocal = inValue; }
extern "C" void ee_screenscpp_lobby_player_set_ready(uint32_t inSlot, bool inValue)
{ gWorld.GetLobbyOptions().mPlayers[inSlot].mIsReady = inValue; }
extern "C" void ee_screenscpp_comms_start_loading()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageStartLoading, {1, TMGetTime()}), kSlotNone); }
extern "C" void ee_screenscpp_um_key_load(uint32_t inKey) { gISKeyboard->GetEntity(kUMKeys[inKey], EEScratch<ISEntity*>(kScratchUMKey)); }
extern "C" bool ee_screenscpp_um_key_just_pressed() { return EEScratch<ISEntity*>(kScratchUMKey)->JustPressed(); }
extern "C" uintptr_t ee_screenscpp_um_current_form(uintptr_t inManager)
{ return (uintptr_t)UMG(inManager)->GetCurrentForm(); }
extern "C" bool ee_screenscpp_form_keyboard_captured(uintptr_t inForm)
{ return reinterpret_cast<UIForm*>(inForm)->GetIsKeyboardCaptured(); }
extern "C" bool ee_screenscpp_loading_saved_game() { return gLoadingSavedGame; }
extern "C" uintptr_t ee_screenscpp_form_manager(uintptr_t inForm)
{ return (uintptr_t)reinterpret_cast<UIForm*>(inForm)->GetManager(); }
extern "C" bool ee_screenscpp_um_in_editor(uintptr_t inManager) { return UMG(inManager)->mInScenarioEditor; }
extern "C" bool ee_screenscpp_game_screen_active() { return gGameScreenActive; }
extern "C" uintptr_t ee_screenscpp_um_editor_form(uintptr_t inManager)
{ return (uintptr_t)static_cast<UIForm*>(UMG(inManager)->GetScenarioEditorForm()); }
extern "C" uintptr_t ee_screenscpp_um_main_game_form(uintptr_t inManager)
{ return (uintptr_t)static_cast<UIForm*>(UMG(inManager)->GetMainGameForm()); }
extern "C" uintptr_t ee_screenscpp_um_new_cancel(uintptr_t inForm)
{ return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_CANCEL, inForm)); }
extern "C" uintptr_t ee_screenscpp_um_new_load_unit() { return (uintptr_t)static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_LOAD_UNIT)); }
// the events' bodies (cpp_events.rs)
extern "C" bool ee_screenscpp_uh_in_game_form()
{ return dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL || dynamic_cast<EEUIFScenarioEditor*>(gUIManager->GetCurrentForm()) != NULL; }
extern "C" long ee_screenscpp_ev_open_form(uintptr_t inForm) { return gUserInterface->OpenForm(reinterpret_cast<UIForm*>(inForm)); }
extern "C" void ee_screenscpp_ev_load_unit(uintptr_t inForm)
{ static_cast<EEUIFMGUnitHack*>(reinterpret_cast<EEUIFMainGame*>(inForm)->mUnitHack)->LoadUnitData(); }
extern "C" uintptr_t ee_screenscpp_um_new_wns_open() { return (uintptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::StatsOpen)); }
extern "C" uintptr_t ee_screenscpp_um_new_wns_cancel() { return (uintptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::StatsCancel)); }
extern "C" void ee_screenscpp_um_new_editor(uintptr_t inManager)
{ UMG(inManager)->mScenarioEditorForm = new EEUIFScenarioEditor(UMG(inManager)); }
extern "C" void ee_screenscpp_um_delete_editor(uintptr_t inManager) { delete UMG(inManager)->GetScenarioEditorForm(); }
extern "C" void ee_screenscpp_um_clear_editor(uintptr_t inManager) { UMG(inManager)->mScenarioEditorForm = NULL; }
extern "C" uint32_t ee_screenscpp_um_form_data_id(uintptr_t inData)
{ return (uint32_t)reinterpret_cast<const UIDbDataForms*>(inData)->mFormID; }
extern "C" uintptr_t ee_screenscpp_um_base_create_form(uintptr_t inManager, uintptr_t inData)
{ return (uintptr_t)static_cast<UIForm*>(UMG(inManager)->UIManager::CreateForm(*reinterpret_cast<const UIDbDataForms*>(inData))); }
extern "C" uintptr_t ee_screenscpp_um_new_main_game(uintptr_t inManager, uintptr_t inData)
{ return (uintptr_t)static_cast<UIForm*>(new EEUIFMainGame(UMG(inManager), *reinterpret_cast<const UIDbDataForms*>(inData))); }
extern "C" uint32_t ee_screenscpp_bt_type(uintptr_t inTable) { return (uint32_t)UMBT(inTable).GetBackgroundType(); }
extern "C" uint32_t ee_screenscpp_bt_form_id(uintptr_t inTable) { return (uint32_t)UMBT(inTable).GetFormID(); }
extern "C" bool ee_screenscpp_bt_attribute1(uintptr_t inTable) { return UMBT(inTable).GetAttribute1() == true; }
extern "C" bool ee_screenscpp_bt_attribute2(uintptr_t inTable) { return UMBT(inTable).GetAttribute2() == true; }
extern "C" bool ee_screenscpp_bt_attribute3(uintptr_t inTable) { return UMBT(inTable).GetAttribute3() == true; }
extern "C" uintptr_t ee_screenscpp_um_form_by_id(uintptr_t inManager, uint32_t inId)
{ return (uintptr_t)UMG(inManager)->GetFormByID(inId); }
extern "C" uintptr_t ee_screenscpp_um_new_generic_background(uintptr_t inManager, uintptr_t inTable, uintptr_t inForm, uint32_t inFlags)
{ return (uintptr_t)static_cast<UIBackground*>(new EEUIBGenericBackground(UMBT(inTable).GetBackgroundID(), UMBT(inTable).GetTextureFile(), GETextureDescription{1, 0, UMG(inManager)->GetUITextureBitDepth(), 0, false, false, UMBT(inTable).GetHasAlpha(), true}, UMBT(inTable).GetLayout(), UMF(inForm), UMBT(inTable).GetDrawPriority(), UMBT(inTable).GetColor(), UMBT(inTable).GetVisible(), UMBT(inTable).GetOpacity(), 1.0f, 1.0f, inFlags)); }
extern "C" uint32_t ee_screenscpp_ct_form_id(uintptr_t inTable) { return (uint32_t)UMCT(inTable).GetFormID(); }
extern "C" uint32_t ee_screenscpp_ct_type(uintptr_t inTable) { return (uint32_t)UMCT(inTable).GetControlType(); }
extern "C" uint32_t ee_screenscpp_um_window_width(uintptr_t inManager)
{ return (uint32_t)UMG(inManager)->GetRasterizer()->GetWindow(UMG(inManager)->GetScreen())->GetWidth(); }
extern "C" uint32_t ee_screenscpp_um_window_height(uintptr_t inManager)
{ return (uint32_t)UMG(inManager)->GetRasterizer()->GetWindow(UMG(inManager)->GetScreen())->GetHeight(); }
extern "C" bool ee_screenscpp_form_exclusive_coords(uintptr_t inForm) { return UMF(inForm)->GetUseExclusiveCoords(); }
extern "C" void ee_screenscpp_um_parent_set(float inLeft, float inTop, float inRight, float inBottom)
{ EEScratch<GERectangle>(kScratchUMParent) = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_ct_layout_rectangle(uintptr_t inTable, uintptr_t inForm)
{ UMCT(inTable).GetLayout().CalculateRectangle(EEScratch<GERectangle>(kScratchUMParent), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm)->GetUseExclusiveCoords()); }
extern "C" void ee_screenscpp_ct_strings(uintptr_t inManager, uintptr_t inTable)
{ UMCT(inTable).SetupStringArray(UMG(inManager), EEScratch<std::vector<UWideString>>(kScratchUMStrings)); }
extern "C" void ee_screenscpp_um_strings_clear() { EEScratch<std::vector<UWideString>>(kScratchUMStrings).clear(); }
extern "C" uintptr_t ee_screenscpp_ct_new_mini_map(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIMiniMap(EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetID(), 0)); }
extern "C" uintptr_t ee_screenscpp_ct_new_status_button(uintptr_t inManager, uintptr_t inTable)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIFMGStatusButton(UMG(inManager), EEScratch<GERectangle>(kScratchUMRect), UMCT(inTable))); }
extern "C" uintptr_t ee_screenscpp_ct_new_error_log(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIShellList(EE_SCREENS_SK_ERROR_LOG, {}, UMCT(inTable).GetID(), std::vector<UWideString>(), UMCT(inTable).GetFont(), false, EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), 0)); }
extern "C" uintptr_t ee_screenscpp_ct_new_attribute(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIFMGAttribute(UMCT(inTable).GetFont(), UMF(inForm), EEScratch<GERectangle>(kScratchUMRect), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetID(), UMCT(inTable).GetTextForeground(), false, UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_status_bar(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIFMGStatusBar(UMCT(inTable).GetFormID(), UMCT(inTable).GetFormIndex(), (UILayoutUtility*)&UMCT(inTable).GetLayout(), 800, 600, EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), false, true, UMCT(inTable).GetLoopMovie())); }
extern "C" uintptr_t ee_screenscpp_ct_new_select_units(uintptr_t inManager, uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICSelectUnits(UMF(inForm), UMCT(inTable).GetID(), UMCT(inTable).GetFormIndex(), static_cast<EEAMouseClick*>(UMG(inManager)->GetMainGameForm()->mMouseClick), static_cast<EEALasso*>(UMG(inManager)->GetMainGameForm()->mLasso))); }
extern "C" uintptr_t ee_screenscpp_ct_new_command_units(uintptr_t inManager, uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICCommandUnits(UMF(inForm), UMCT(inTable).GetID(), UMCT(inTable).GetFormIndex(), static_cast<EEAMouseClick*>(UMG(inManager)->GetMainGameForm()->mMouseClick))); }
extern "C" uintptr_t ee_screenscpp_ct_new_portrait(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICPortrait(UMF(inForm), EEScratch<GERectangle>(kScratchUMRect), UMCT(inTable).GetID(), UMCT(inTable).GetFormIndex(), false)); }
extern "C" void ee_screenscpp_ct_setup_font(uintptr_t inManager, uintptr_t inTable)
{ UMCT(inTable).SetupFont(UMG(inManager), EEScratch<GERectangle>(kScratchUMRect), true); }
extern "C" uintptr_t ee_screenscpp_ct_new_cb_list_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EECBUIListBox(UMCT(inTable), UMCT(inTable).GetID(), EEScratch<std::vector<UWideString>>(kScratchUMStrings), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness(), UMCT(inTable).GetStartChecked())); }
extern "C" uintptr_t ee_screenscpp_ct_new_rooms_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICWonNetRoomsBox(UMCT(inTable).GetID(), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetWidth(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetHeight(), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_games_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICWonNetGamesBox(UMCT(inTable).GetID(), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetWidth(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetHeight(), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_stat_box(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICWonNetStatBox(UMCT(inTable).GetID(), UMCT(inTable).GetFont(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetWidth(), (unsigned long)EEScratch<GERectangle>(kScratchUMRect).GetHeight(), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
extern "C" uintptr_t ee_screenscpp_ct_new_saved_group(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIButton*>(new EEUIShellButton(EE_SCREENS_SK_SAVED_GROUP, {}, UMCT(inTable).GetID(), NULL, NULL, NULL, EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), 0)); }
extern "C" uintptr_t ee_screenscpp_ct_normal_bitmap(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)UMCT(inTable).CreateNormalBitmap(UMF(inForm)->GetManager(), EEScratch<GERectangle>(kScratchUMRect)); }
extern "C" uintptr_t ee_screenscpp_ct_pressed_bitmap(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)UMCT(inTable).CreatePressedBitmap(UMF(inForm)->GetManager(), EEScratch<GERectangle>(kScratchUMRect)); }
extern "C" void ee_screenscpp_um_button_set_normal(uintptr_t inButton, uintptr_t inBitmap)
{ reinterpret_cast<UIButton*>(inButton)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_um_button_set_pressed(uintptr_t inButton, uintptr_t inBitmap)
{ reinterpret_cast<UIButton*>(inButton)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_um_bitmap_dereference(uintptr_t inBitmap)
{ reinterpret_cast<GEBitmap*>(inBitmap)->Dereference(); }
extern "C" uintptr_t ee_screenscpp_ct_new_pgs_graph(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new UIGraph(UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness(), EeUiGraphKind::PostGame, ee_screens_pgs_format)); }
extern "C" void ee_screenscpp_ct_graph_font(uintptr_t inGraph, uintptr_t inTable)
{ reinterpret_cast<UIGraph*>(inGraph)->SetFont(UMCT(inTable).GetFont()); }
extern "C" void ee_screenscpp_ct_graph_font_color(uintptr_t inGraph, uintptr_t inTable)
{ reinterpret_cast<UIGraph*>(inGraph)->SetFontColor(UMCT(inTable).GetTextForeground()); }
extern "C" uintptr_t ee_screenscpp_ct_new_ping(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUICPing(UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect).GetTopLeft(), UMF(inForm), UMCT(inTable).GetFormIndex())); }
extern "C" uintptr_t ee_screenscpp_um_button_control(uintptr_t inButton)
{ return (uintptr_t)static_cast<UIControl*>(reinterpret_cast<UIButton*>(inButton)); }

// the manager's construction as the game's start makes it (Empire Earth.cpp: ee/EEGameUI.h)
static const bool sManagerRegistered =
    (gEEGameUI.mNewManager = [](GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                                const UString& inUnused) { return new EEUIManager(inRasterizer, inNumberOfSurfaces,
                                                                                  inScreen, inUnused); },
     true);
// the icon bar's and chat line's shells over the factory's rectangle (shell_factories.rs, UI lane round 12)
extern "C" uintptr_t ee_screenscpp_sf_icon_bar_new(uintptr_t inTable, uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIShellControl(EE_SCREENS_SK_ICON_BAR, std::vector<int64_t>(), UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), 0)); }
extern "C" uintptr_t ee_screenscpp_sf_chat_line_new(uintptr_t inTable, uintptr_t inForm, int64_t inOpacityBits)
{ return (uintptr_t)static_cast<UIControl*>(new EEUIShellControl(EE_SCREENS_SK_CHAT_LINE, std::vector<int64_t>(1, inOpacityBits), UMCT(inTable).GetID(), EEScratch<GERectangle>(kScratchUMRect), UMF(inForm), UMCT(inTable).GetFormIndex(), UMCT(inTable).GetBorderThickness())); }
