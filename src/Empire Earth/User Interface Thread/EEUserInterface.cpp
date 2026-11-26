// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.cpp (__FILE__ 0x4cf450). The UI thread: 0x276631..0x27a37e
// (EEUIPGSLineGraph.cpp ends at 0x276631). The ctor, ??_G, dtor, the thread's handlers (in address order), Run,
// HandleMessage (the 0x401..0x443 switch), the Post* members, the window procedure, then the template COMDATs of
// the message vector. Unit flags: the exe module set. Names are provisional unless the header says otherwise.
#include <windows.h>
#include "platform/PlatSim.h"
#include <list>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEGameSound.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEUIShell.h"
#include "ee/EESMMediaStrings.h"
#include <locale.h>
#include "lle/UIStatusBar.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EERenderObject.h"
#include "ee/EEROUserInterface.h"
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
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveLoad.h"
#include "ee/EEUIEWonNetEvent.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEARust.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEStatisticsRecord.h"
// free functions of other units (declared where they are used, as their own units do) (provisional names)
bool EEIsLobbyOpen();                       // 0x297eca (the external lobby is running)
void EESetQuitFlag();
// CivBuilderEditCivilization 0x280faf, CivBuilderCreateCivilization 0x280ffd: ee/EECivilizationBuilder.h; EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h
void EELobbyShutdown();
extern bool gSightUpdatesEnabled;           // 0x4c76ec (set before the tested scenario starts) (provisional name)
#include "ee/EEStringIDs.h"
#include "ee/EEScreensShims.h"

static const float kGameViewLeft = 0.0f, kGameViewRight = 1600.0f;  // the game view below the top bar (.rdata
static const float kGameViewTop = 35.0f, kGameViewBottom = 1085.0f; // 0x44ba2c..0x44ba38: separate words)

// LeaveMap, CancelGame, CloseScenarioEditor, EnterScenarioEditor, StartLoadingFromEditor, ShowCivilizationBuilder and
// RestartGame are Rust's whole (ee_screens ui_game.rs; the thread's messages run them by ee_screens_ui_member).
// The UI thread's game starts' host (EE_SCREENS_UG_OP_*): Rust's (ui_game_calls.rs and the modules before it)
// over the one-statement shims below (UI lane round 11)
#define UG_UI(u) static_cast<EEUserInterface*>((void*)(u))
extern "C" void ee_screenscpp_ug_screen_active(bool inActive) { gGameScreenActive = inActive; }
extern "C" void ee_screenscpp_ug_path_reset() { gPathManager.Reset(); }
extern "C" void ee_screenscpp_ug_sight_on() { gSightUpdatesEnabled = true; }
extern "C" void ee_screenscpp_ug_restart_units() { gWorld.RestartUnits(); }
extern "C" void ee_screenscpp_ug_resize_game(uintptr_t inUi, bool inA, bool inB)
{ UG_UI(inUi)->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, inA, inB); }
extern "C" void ee_screenscpp_ug_resize_menu(uintptr_t inUi, bool inA, bool inB)
{ UG_UI(inUi)->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), inA, inB); }
extern "C" void ee_screenscpp_ug_physics_post(uint64_t inMessage)
{ gPhysics->mThread->Post((unsigned long)inMessage, NULL, NULL, true); }
extern "C" void ee_screenscpp_ug_music() { gMusicManager->Start(); }
extern "C" void ee_screenscpp_ug_prepare_save() { gWorld.mMap->PrepareSave(); }
extern "C" int64_t ee_screenscpp_ug_save_scenario()
{ return SaveGame(UString("ScenDefault") + gScenarioExtension, TMGetTime(), true, UString("data\\") + UString("Scenarios\\")); }
extern "C" void ee_screenscpp_ug_save_error()
{ gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true); }
extern "C" void ee_screenscpp_ug_max_players() { gWorld.SetNumPlayers(gWorld.mMaxPlayers); }
extern "C" void ee_screenscpp_ug_collect_computers() { gArtificialIntelligence->CollectComputerPlayers(); }
extern "C" bool ee_screenscpp_ug_timer_running() { return gPhysics->mTimerRunning; }
extern "C" void ee_screenscpp_ug_state_clear() { gGameState.Clear(); }
extern "C" void ee_screenscpp_ug_options_defaults() { gGameOptions.SetDefaults(); }
extern "C" uint64_t ee_screenscpp_ug_file_name_length(uintptr_t inFile)
{ return (uint64_t)reinterpret_cast<FSFile*>(inFile)->GetFile().GetLength(); }
extern "C" uint64_t ee_screenscpp_ug_file_scenario_at(uintptr_t inFile)
{ return (uint64_t)reinterpret_cast<FSFile*>(inFile)->GetFile().Find(gScenarioExtension, 0); }
extern "C" void ee_screenscpp_ug_post_load(bool inA, bool inB) { gDataPreLoading->PostLoadGame(inA, inB); }
extern "C" void ee_screenscpp_ug_delete_file(uintptr_t inFile) { delete reinterpret_cast<FSFile*>(inFile); }
extern "C" void ee_screenscpp_ug_update_interface(uintptr_t inUi) { UG_UI(inUi)->UpdateInterface(); }
extern "C" bool ee_screenscpp_ug_free_camera() { return gFreeCamera; }
extern "C" bool ee_screenscpp_ug_sim_run() { return gPlatSimRun; }
extern "C" void ee_screenscpp_ug_wait_preload() { gDataPreLoading->WaitForMessageQueue(); }
extern "C" void ee_screenscpp_ug_world_clear() { gWorld.Clear(); }
extern "C" bool ee_screenscpp_ug_one_record() { return gWorld.mStatistics.GetRecords().size() == 1; }
extern "C" bool ee_screenscpp_ug_has_communications() { return gCommunications != NULL; }
extern "C" bool ee_screenscpp_ug_is_client() { return gCommunications->IsClient(); }
extern "C" bool ee_screenscpp_ug_server_lost_after_loading()
{ return static_cast<EEClient*>(gCommunications)->mServerLostAfterLoading; }
extern "C" bool ee_screenscpp_ug_lobby_open() { return EEIsLobbyOpen(); }
extern "C" void ee_screenscpp_ug_create_editor() { gUIManager->CreateScenarioEditor(); }
extern "C" int64_t ee_screenscpp_ug_load_scendefault()
{ return EELoadSavedGameFile(UString("ScenDefault") + gScenarioExtension, true, true, UString("data\\") + UString("Scenarios\\"), 0); }
extern "C" void ee_screenscpp_ug_stop_one_shots() { gSoundManager->StopAllOneShotSounds(); }
extern "C" bool ee_screenscpp_ug_graphics_paused() { return gGraphics->mIsPaused; }
extern "C" bool ee_screenscpp_ug_can_buy_civ() { return gWorld.mGameOptions.CanBuyCivilization(); }
extern "C" bool ee_screenscpp_ug_custom_civs() { return gWorld.mGameOptions.mAllowCustomCivs; }

// The action queues' host (actions_ffi.rs picks): an action's reference, release, run, undo and undoable flag
extern "C" void ee_screenscpp_action_reference(EEAction* inAction) { inAction->Reference(); }
extern "C" void ee_screenscpp_action_release(EEAction* inAction) { inAction->Dereference(); }
extern "C" void ee_screenscpp_action_run(EEAction* inAction) { inAction->Virtual4(); }
extern "C" void ee_screenscpp_action_undo(EEAction* inAction) { inAction->Virtual8(); }
extern "C" bool ee_screenscpp_action_undoable(EEAction* inAction) { return inAction->mIsUndoable; }

// The UI thread's message host (EE_SCREENS_UI_OP_*): Rust's (ui_message_calls.rs and the modules before it) over
// the one-statement shims below (UI lane round 11)
#define UIM(u) static_cast<EEUserInterface*>((void*)(u))
#define UIM_CAP(c) reinterpret_cast<EEScreenCapture*>(c)
#define UIM_CHAT(e) reinterpret_cast<EeScreensChatEntry*>(e)
extern "C" void ee_screenscpp_uim_pause(uintptr_t inUi) { UIM(inUi)->PauseGraphics(); }
extern "C" void ee_screenscpp_uim_resume(uintptr_t inUi) { UIM(inUi)->ResumeGraphics(); }
extern "C" void ee_screenscpp_uim_status_value(uintptr_t inForm, uint64_t inControl, int64_t inValue)
{ static_cast<UIStatusBar*>(((UIForm*)inForm)->GetControl((unsigned long)inControl))->SetCurrentRangeValue(inValue); }
extern "C" void ee_screenscpp_uim_resize_game(uintptr_t inUi)
{ UIM(inUi)->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true); }
extern "C" void ee_screenscpp_uim_resize_menu(uintptr_t inUi)
{ UIM(inUi)->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false); }
extern "C" void ee_screenscpp_uim_screen_active() { gGameScreenActive = true; }
extern "C" void ee_screenscpp_uim_post_load_game() { gDataPreLoading->PostLoadGame(false, false); }
extern "C" bool ee_screenscpp_uim_has_manager() { return gUIManager != NULL; }
extern "C" uintptr_t ee_screenscpp_uim_main_form() { return (uintptr_t)gUIManager->GetMainGameForm(); }
extern "C" void ee_screenscpp_uim_main_rust(uintptr_t inMain, uint32_t inWhich, int64_t inA)
{ ((EEUIFMainGame*)inMain)->Rust(inWhich, inA); }
extern "C" bool ee_screenscpp_uim_gfx_post_pause() { return gGraphics->PostPause() == kErrorOK; }
extern "C" bool ee_screenscpp_uim_gfx_paused() { return gGraphics->mIsPaused; }
extern "C" void ee_screenscpp_uim_gfx_post_resume() { gGraphics->PostResume(); }
extern "C" bool ee_screenscpp_uim_capture_event(uintptr_t inCapture)
{ return TSScheduler::CreateEvent(UIM_CAP(inCapture)->mCaptureComplete) == keseOK; }
extern "C" void ee_screenscpp_uim_capture_post(uintptr_t inCapture) { gGraphics->PostScreenShot(UIM_CAP(inCapture)); }
extern "C" void ee_screenscpp_uim_capture_wait(uintptr_t inCapture)
{ UIM_CAP(inCapture)->mCaptureComplete->Wait(kTSWaitInfinite); }
extern "C" void ee_screenscpp_uim_capture_event_free(uintptr_t inCapture)
{ TSScheduler::DestroyEvent(UIM_CAP(inCapture)->mCaptureComplete); }
extern "C" int64_t ee_screenscpp_uim_capture_width(uintptr_t inCapture) { return (int64_t)UIM_CAP(inCapture)->mWidth; }
extern "C" void ee_screenscpp_uim_capture_surface_release(uintptr_t inCapture)
{ UIM_CAP(inCapture)->mSurface->Dereference(); }
extern "C" void ee_screenscpp_uim_capture_delete(uintptr_t inCapture) { delete UIM_CAP(inCapture); }
extern "C" void ee_screenscpp_uim_verify(uint32_t inLine)
{ ESVerify(__FILE__, inLine, "TSScheduler::CreateEvent(theCapture->mCaptureComplete) == keseOK"); }
extern "C" void ee_screenscpp_uim_post_gfx_all() { EEPostGraphicsAll(); }
extern "C" bool ee_screenscpp_uim_error_has_callback(uintptr_t inData)
{ return reinterpret_cast<EEUIFErrorCallbackData*>(inData)->mCallback != NULL; }
extern "C" void ee_screenscpp_uim_error_callback(uintptr_t inData)
{ reinterpret_cast<EEUIFErrorCallbackData*>(inData)->mCallback(static_cast<UIForm*>(reinterpret_cast<EEUIFErrorCallbackData*>(inData)->mData)); }
extern "C" void ee_screenscpp_uim_delete_form(uintptr_t inForm) { delete (UIForm*)inForm; }
extern "C" void ee_screenscpp_uim_delete_error_data(uintptr_t inData)
{ delete reinterpret_cast<EEUIFErrorCallbackData*>(inData); }
extern "C" void ee_screenscpp_uim_chat_show(uintptr_t inMain, uintptr_t inEntry)
{ ((EEUIFMainGame*)inMain)->DisplayChat(GEColor(UIM_CHAT(inEntry)->head.colour[0], UIM_CHAT(inEntry)->head.colour[1], UIM_CHAT(inEntry)->head.colour[2]), UWideString(UIM_CHAT(inEntry)->name), UWideString(UIM_CHAT(inEntry)->text), UIM_CHAT(inEntry)->head.time, UIM_CHAT(inEntry)->head.kind); }
extern "C" void ee_screenscpp_uim_show_information(uintptr_t inMain, uintptr_t inInfo)
{ ((EEUIFMainGame*)inMain)->ShowInformation(reinterpret_cast<EEUIFMGInformation*>(inInfo)); }
extern "C" void ee_screenscpp_uim_delete_dialog(uintptr_t inDialog)
{ delete reinterpret_cast<EESMDialogMessage*>(inDialog); }
extern "C" void ee_screenscpp_uim_icon_bar_posted_clear(uintptr_t inUi) { UIM(inUi)->mIconBarUpdatePosted = false; }
extern "C" void ee_screenscpp_uim_change_rasterizer(uintptr_t inUi, int64_t inA, int64_t inB)
{ UIM(inUi)->ChangeRasterizer(inA, inB); }
extern "C" void ee_screenscpp_uim_post_event(uintptr_t inEvent)
{ gUserInterface->PostEvent(reinterpret_cast<UIEvent*>(inEvent), NULL); }
extern "C" void ee_screenscpp_uim_select_tab(uint8_t inTab, int64_t inB)
{ gUIManager->GetScenarioEditorForm()->SelectTab(inTab, inB); }
extern "C" void ee_screenscpp_uim_saved_group(uintptr_t inMain, uint64_t inControl)
{ EEUIShell::Op(((EEUIFMainGame*)inMain)->GetControl((unsigned long)inControl), EE_SCREENS_SKOP_SG_REFRESH); }
extern "C" void ee_screenscpp_uim_select_server(uintptr_t inUi) { UIM(inUi)->OpenSelectServer(); }
extern "C" void ee_screenscpp_uim_motd(uintptr_t inUi, uintptr_t inA, uintptr_t inB)
{ UIM(inUi)->ShowMessageOfTheDay(*reinterpret_cast<UWideString*>(inA), *reinterpret_cast<UWideString*>(inB)); }
extern "C" void ee_screenscpp_uim_delete_wstring(uintptr_t inText) { delete reinterpret_cast<UWideString*>(inText); }

// (the posts the game makes, ResumeGraphics and HandleEvent are inline: ee/EEUserInterface.h)

// The window events' one-call ops (EE_SCREENS_UI_EV_*; ui_calls.rs ui_event): the globals' NULL tests, the lobby, the
// volumes, the input devices, the mouse pointer's thread, the input method
extern "C" bool ee_screenscpp_ui_has_keyboard() { return gISKeyboard != NULL; }
extern "C" bool ee_screenscpp_ui_has_mouse() { return gISMouse != NULL; }
extern "C" bool ee_screenscpp_ui_has_window_and_mouse() { return gGameWindow != NULL && gMouse != NULL; }
extern "C" bool ee_screenscpp_ui_has_pointer() { return gMousePointer != NULL; }
extern "C" bool ee_screenscpp_ui_has_window() { return gGameWindow != NULL; }
extern "C" bool ee_screenscpp_ui_lobby_open() { return EEIsLobbyOpen(); }
extern "C" void ee_screenscpp_ui_lobby_shutdown() { EELobbyShutdown(); }
extern "C" void ee_screenscpp_ui_set_music_volume(float inVolume) { gConductor->GetConductor().SetVolume(inVolume); }
extern "C" void ee_screenscpp_ui_set_master_volume(float inVolume) { gSoundManager->SetMasterVolume(inVolume); }
extern "C" void ee_screenscpp_ui_mute(bool inMuted) { SPIMSetMuted(inMuted); }   // port: "Mute in Background"
extern "C" void ee_screenscpp_ui_center_mouse()
{ gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() >> 1), (float)(gGameWindow->GetHeight() >> 1))); }
extern "C" void ee_screenscpp_ui_pointer_post(int64_t inMessage) { gMousePointer->mThread->Post(inMessage, NULL, NULL, true); }
extern "C" bool ee_screenscpp_ui_pointer_shown() { return gMousePointer->mIsShown; }
extern "C" bool ee_screenscpp_ui_mute_in_background() { return gGamePreferences.mSettings.mMuteInBackground; }   // port
extern "C" bool ee_screenscpp_ui_full_screen() { return gGamePreferences.mSettings.GetFullScreen(); }
extern "C" void ee_screenscpp_ui_minimize() { PlatWindowMinimize(gGameWindow->GetOSRepresentation()); }   // CloseWindow
// the committed composition
extern "C" void ee_screenscpp_ui_ime_result(uintptr_t inText) { gUIManager->AddIMEResultString(reinterpret_cast<const char*>(inText)); }
extern "C" void ee_screenscpp_ui_translated_key(uintptr_t inText) { gUIManager->UpdateTranslatedKey(reinterpret_cast<const char*>(inText), 1); }

// ---- the shims of the moved code (UI lane round 7: ee_game screens ui_calls.rs, game_cpp.rs), each one C++
// statement
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
// shims4
extern "C" void ee_screenscpp_gfx_post_pause() { gGraphics->PostPause(); }
extern "C" bool ee_screenscpp_gfx_paused() { return gGraphics->mIsPaused; }
extern "C" void ee_screenscpp_sleep(uint32_t inMilliseconds) { PlatSleep(inMilliseconds); }
extern "C" bool ee_screenscpp_pointer_shown() { return gMousePointer->mIsShown; }
extern "C" uint32_t ee_screenscpp_game_window_width() { return (uint32_t)gGameWindow->GetWidth(); }
extern "C" uint32_t ee_screenscpp_game_window_height() { return (uint32_t)gGameWindow->GetHeight(); }
extern "C" void ee_screenscpp_pointer_post(uint32_t inMessage)
{ gMousePointer->mThread->Post(inMessage, NULL, NULL, true); }
extern "C" void ee_screenscpp_pointer_post_surface(uint32_t inMessage)
{ gMousePointer->mThread->Post(inMessage, gRasterizer->GetPrimarySurface(gScreenIndex), NULL, true); }
extern "C" void ee_screenscpp_manager_relayout_all() { gUIManager->RelayoutAllForms(); }
extern "C" uintptr_t ee_screenscpp_mini_map() { return (uintptr_t)gMiniMap; }
extern "C" void ee_screenscpp_world_write_lock() { gWorld.mLock->WriteLock(); }
extern "C" void ee_screenscpp_world_write_unlock() { gWorld.mLock->WriteUnlock(); }
extern "C" void ee_screenscpp_world_read_lock() { gWorld.mLock->ReadLock(); }
extern "C" void ee_screenscpp_world_read_unlock() { gWorld.mLock->ReadUnlock(); }
extern "C" void ee_screenscpp_mini_map_redraw_all() { gMiniMap->RedrawAll(); }
extern "C" void ee_screenscpp_game_window_client_origin() { gGameWindow->GetClientOrigin(); }
extern "C" uint32_t ee_screenscpp_rasterizer_screen_count() { return (uint32_t)gRasterizer->GetScreenCount(); }
extern "C" void ee_screenscpp_rasterizer_release_screen(uint32_t inIndex)
{ gRasterizer->GetScreen(inIndex)->Release(gGameWindow); }
extern "C" int32_t ee_screenscpp_rasterizer_deactivate() { return (int32_t)gRasterizer->Deactivate(); }
extern "C" void ee_screenscpp_verify_rasterizer_deactivated(int32_t inResult) { ESVERIFY_AT(inResult == keseOK, 252); }
extern "C" void ee_screenscpp_rasterizer_select(int32_t inIndex) { gRasterizer = GERasterizer::GetRasterizer(inIndex); }
extern "C" void ee_screenscpp_gfx_post_set_rasterizer(int32_t inScreen)
{ gGraphics->PostSetRasterizer(gRasterizer, inScreen); }
extern "C" void ee_screenscpp_manager_change_rasterizer() { gUIManager->ChangeRasterizer(gRasterizer); }
extern "C" uint32_t ee_screenscpp_menu_width() { return (uint32_t)EEMenuWidth(); }
extern "C" uint32_t ee_screenscpp_menu_height() { return (uint32_t)EEMenuHeight(); }
extern "C" void ee_screenscpp_manager_lock_for_copy() { gUIManager->LockUIForCopyData(); }
extern "C" void ee_screenscpp_manager_unlock_for_copy() { gUIManager->UnlockUIForCopyData(); }
extern "C" bool ee_screenscpp_manager_form_visible(uint32_t inForm)
{ return gUIManager->IsFormVisible(gUIManager->GetFormByID(inForm)) == true; }
extern "C" void ee_screenscpp_control_set_display_mode(uintptr_t inControl, uint32_t inMode)
{ C(inControl)->SetDisplayMode(inMode); }
extern "C" void ee_screenscpp_label_append_wide(uintptr_t inLabel, uintptr_t inText)
{ static_cast<UITextLabel*>(C(inLabel))->AppendText(*reinterpret_cast<const UWideString*>(inText)); }
extern "C" uintptr_t ee_screenscpp_select_server_event_new()
{ return (uintptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::SsOpenScreen)); }
extern "C" void ee_screenscpp_manager_form_set_open_event(uint32_t inForm, uintptr_t inEvent)
{ gUIManager->GetFormByID(inForm)->SetFormOpenEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_manager_form_set_close_event(uint32_t inForm, uintptr_t inEvent)
{ gUIManager->GetFormByID(inForm)->SetFormCloseEvent(reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" bool ee_screenscpp_check_box_checked(uintptr_t inCheck)
{ return static_cast<UICheckBox*>(C(inCheck))->GetIsChecked() == true; }
extern "C" void ee_screenscpp_won_password_from_edit(uintptr_t inEdit)
{ gGamePreferences.mSettings.mWonNetPassword = UString(static_cast<UIEditBox*>(C(inEdit))->GetText(0)); }
extern "C" void ee_screenscpp_won_password_clear() { gGamePreferences.mSettings.mWonNetPassword = UString(""); }
extern "C" void ee_screenscpp_multiplayer_string_from_edit(uintptr_t inEdit)
{ gMultiplayerString = UString(static_cast<UIEditBox*>(C(inEdit))->GetText(0)); }
extern "C" void ee_screenscpp_manager_pop_to(uint32_t inForm) { gUIManager->PopTo(gUIManager->GetFormByID(inForm)); }
extern "C" void ee_screenscpp_manager_push(uint32_t inForm) { gUIManager->Push(gUIManager->GetFormByID(inForm)); }
extern "C" uintptr_t ee_screenscpp_manager_form(uint32_t inForm) { return (uintptr_t)gUIManager->GetFormByID(inForm); }
extern "C" void ee_screenscpp_keyboard_refresh() { gISKeyboard->Refresh(); }
extern "C" void ee_screenscpp_mouse_input_lock_enter() { gMouse->mInputLock->Enter(); }
extern "C" void ee_screenscpp_mouse_refresh() { gISMouse->Refresh(); }
extern "C" void ee_screenscpp_mouse_update() { gMouse->Update(); }
extern "C" void ee_screenscpp_mouse_input_lock_leave() { gMouse->mInputLock->Leave(); }
extern "C" bool ee_screenscpp_lobby_shown() { return EELobbyIsShown(); }
extern "C" void ee_screenscpp_manager_update_ui() { gUIManager->UpdateUI(gMouse->mPosition); }
extern "C" void ee_screenscpp_actions_process() { ee_screens_actions_process(); }
extern "C" void ee_screenscpp_ui_resume_graphics(uintptr_t inUi)
{ reinterpret_cast<EEUserInterface*>(inUi)->ResumeGraphics(); }
extern "C" void ee_screenscpp_continents_clear() { gContinentManager.Clear(gWorld.mMap); }
extern "C" void ee_screenscpp_continents_build_regions() { gContinentManager.BuildRegions(gWorld.mMap); }
extern "C" void ee_screenscpp_player_register_herds(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->RegisterHerds(); }
extern "C" void ee_screenscpp_player_assign_herds(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->AssignHerds(); }
extern "C" void ee_screenscpp_player_register_towns(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->RegisterTowns(); }
extern "C" void ee_screenscpp_player_assign_towns(uint32_t inPlayer) { gWorld.mPlayers[inPlayer]->AssignTowns(); }
extern "C" void ee_screenscpp_player_refresh_resource_centres(uint32_t inPlayer)
{ gWorld.mPlayers[inPlayer]->mComputerPlayer->RefreshResourceCenters(); }
extern "C" void ee_screenscpp_player_refresh_town_groups(uint32_t inPlayer)
{ gWorld.mPlayers[inPlayer]->RefreshTownGroups(); }
extern "C" void ee_screenscpp_mini_map_redraw() { gMiniMap->Redraw(); }
extern "C" void ee_screenscpp_ambient_enable_from_settings()
{ gAmbientSoundManager.SetEnabled(gGamePreferences.mSettings.mAmbientSounds); }
extern "C" void ee_screenscpp_ambient_restart() { gAmbientSoundManager.Restart(); }
extern "C" void ee_screenscpp_world_options_set_load_type(int32_t inLoadType)
{ gWorld.mGameOptions.mLoadType = inLoadType; }
extern "C" void ee_screenscpp_world_options_set_map_type(EeScreensShimText inName)
{ gWorld.mGameOptions.mMapType = EEShimText(inName); }
extern "C" int32_t ee_screenscpp_header_load_file(uintptr_t inHeader, uintptr_t inFile)
{ return (int32_t)reinterpret_cast<EEPSaveGameHeader*>(inHeader)->LoadFromFile(*reinterpret_cast<FSFile*>(inFile)); }
extern "C" void ee_screenscpp_server_set_humans(uint32_t inHumans)
{ static_cast<EEServer*>(gCommunications)->SetNumHumanPlayers(inHumans); }
extern "C" void ee_screenscpp_server_set_multiplayer(bool inMultiplayer)
{ static_cast<EEServer*>(gCommunications)->SetIsMultiplayerSession(inMultiplayer); }
extern "C" void ee_screenscpp_options_map_type_from_file(uintptr_t inFile)
{ gGameOptions.mMapType = UWideString(reinterpret_cast<FSFile*>(inFile)->GetFile()); }
extern "C" void ee_screenscpp_mouse_resume() { gISMouse->Resume(); }
extern "C" void ee_screenscpp_keyboard_resume() { gISKeyboard->Resume(); }
extern "C" void ee_screenscpp_mouse_suspend() { gISMouse->Suspend(); }
extern "C" void ee_screenscpp_keyboard_suspend() { gISKeyboard->Suspend(); }
extern "C" void ee_screenscpp_set_locale_all(EeScreensShimText inLocale) { setlocale(LC_ALL, EEShimString(inLocale)); }
extern "C" void ee_screenscpp_set_locale_numeric_english() { setlocale(LC_NUMERIC, "English"); }
extern "C" void ee_screenscpp_set_locale_ctype_c() { setlocale(LC_CTYPE, "C"); }
extern "C" void ee_screenscpp_query_locale() { setlocale(LC_ALL, NULL); }
extern "C" void ee_screenscpp_ui_render_object_new() { gU56534C = new EEROUserInterface(gUIManager); }
extern "C" void ee_screenscpp_gfx_push_ui_render_object() { gGraphics->PostPushRenderObject(gU56534C); }
extern "C" void ee_screenscpp_gfx_set_flag54(bool inFlag) { gGraphics->PostSetFlag54(inFlag); }
extern "C" void ee_screenscpp_gfx_post_resume() { gGraphics->PostResume(); }
extern "C" void ee_screenscpp_fpu_set_precision_53() { UMSetFloatingPointPrecision(kFPPrecision53); }
extern "C" bool ee_screenscpp_fpu_precision_is_53() { return UMGetFloatingPointPrecision() == kFPPrecision53; }
extern "C" uint32_t ee_screenscpp_fpu_rounding_mode() { return (uint32_t)UMGetFloatingPointRoundingMode(); }
extern "C" void ee_screenscpp_fpu_set_rounding_mode(uint32_t inMode) { UMSetFloatingPointRoundingMode(inMode); }
extern "C" void ee_screenscpp_camera_update(float inSeconds) { gWorld.mCameraControl.Update(inSeconds); }
extern "C" void ee_screenscpp_gfx_release_scene() { gGraphics->PostReleaseScene(); }
extern "C" void ee_screenscpp_gfx_wait_50a() { gGraphics->PostMessage50AAndWait(); }
extern "C" void ee_screenscpp_physics_post_init() { gPhysics->PostInit(); }
extern "C" void ee_screenscpp_physics_run_owed_ticks() { gPhysics->RunOwedTicks(); }
extern "C" void ee_screenscpp_physics_init() { gPhysics->Init(); }
extern "C" uintptr_t ee_screenscpp_communications() { return (uintptr_t)gCommunications; }
extern "C" void ee_screenscpp_new_client() { gCommunications = new EEClient; }
extern "C" void ee_screenscpp_client_start_threads() { static_cast<EEClient*>(gCommunications)->StartThreads(); }
extern "C" uintptr_t ee_screenscpp_boot_client_new(int32_t inId)
{ return (uintptr_t)static_cast<EEAction*>(new EEABootClientFromCanceledGame((long)inId, true)); }
extern "C" void ee_screenscpp_music_pause() { gMusicManager->Pause(); }
extern "C" void ee_screenscpp_ambient_stop_sounds() { gAmbientSoundManager.StopSounds(); }
extern "C" void ee_screenscpp_ambient_pause() { gAmbientSoundManager.Pause(); }
extern "C" void ee_screenscpp_chat_log_clear() { ee_screens_chat_log_clear(); }
extern "C" void ee_screenscpp_civ_builder_button_blinking(bool inBlinking)
{ EESetCivBuilderButtonBlinking(inBlinking); }
extern "C" void ee_screenscpp_player_civilization_chosen()
{ gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationChosen = true; }
extern "C" void ee_screenscpp_player_civilization_locked()
{ gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationLocked = true; }
extern "C" void ee_screenscpp_event_process(uintptr_t inEvent, uintptr_t inEntity)
{ reinterpret_cast<UIEvent*>(inEvent)->ProcessEvent(NULL, reinterpret_cast<UIEntity*>(inEntity)); }
extern "C" void ee_screenscpp_quick_save_set_load(uintptr_t inAction)
{ reinterpret_cast<EEAQuickSaveLoad*>(inAction)->mIsLoad = true; }
extern "C" void ee_screenscpp_chat_entry_text_from_dialog(uintptr_t inEntry, uintptr_t inDialog)
{ ee_screens_chat_entry_set_text(inEntry, (const char*)reinterpret_cast<EESMDialogMessage*>(inDialog)->mText); }
extern "C" void ee_screenscpp_set_quit_flag() { EESetQuitFlag(); }
extern "C" uintptr_t ee_screenscpp_boot_client_host_gone_new()
{ return (uintptr_t)static_cast<EEAction*>(new EEABootClientFromCanceledGame(IDS_HOST_OF_THIS_GAME_HAS_GONE)); }
extern "C" void ee_screenscpp_client_disconnect() { static_cast<NESynchronousClient*>(gCommunications)->Disconnect(); }
extern "C" uintptr_t ee_screenscpp_conductor() { return (uintptr_t)gConductor; }
extern "C" float ee_screenscpp_conductor_volume() { return gConductor->GetConductor().GetVolume(); }
extern "C" uintptr_t ee_screenscpp_sound_manager() { return (uintptr_t)gSoundManager; }
extern "C" float ee_screenscpp_master_volume() { return gSoundManager->GetMasterVolume(); }
extern "C" void ee_screenscpp_ime_composition(EeScreensShimText inText, EeScreensShimText inAttributes, uint32_t inCursor)
{ gUIManager->ChangeIMECompositionString(EEShimText(inText), EEShimString(inAttributes), (unsigned long)inCursor); }
// end shims4

// (the window's event pump and native message hook: Empire Earth.cpp, which installs them)

// the number of humans in the session (SetLoadProgress's test: ee_screens ui_objects_ffi.rs)
extern "C" int64_t ee_screenscpp_ui_humans()
{
    return gCommunications != NULL ? gCommunications->GetNumHumanPlayers() : 0;
}

// the posts' references and the graphics thread's resume (ee_screens ui_objects_ffi.rs)
extern "C" void ee_screenscpp_ui_event_reference(void* inEvent) { static_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_screenscpp_ui_object_reference(void* inObject) { static_cast<UReferenceCounter*>(inObject)->Reference(); }
extern "C" void ee_screenscpp_ui_graphics_post_resume() { gGraphics->PostResume(); }
extern "C" bool ee_screenscpp_ui_graphics_paused() { return gGraphics->mIsPaused; }
