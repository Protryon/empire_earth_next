// C:\EEX\Empire Earth\Civilization Builder\EECivilizationBuilder.cpp (file name inferred: no __FILE__ or RTTI).
// 0x27efca..0x27fdfe: the civilization builder forms' helpers (form 7 the builder, 0xf the name dialog, 0x10/0x11
// the save/load dialogs, 0x23 the in-game civilization selection) and EEProcessTimerExpired. The builder is Rust's
// whole (ee_screens civ_whole.rs with civ_edit.rs, the edited civilization, and civ_list.rs, the list box): its
// helpers, its state, the edited civilization, its files' writing, its open, close, buys, returns, loads, deletes and
// accept. This unit keeps the entry points the game calls and the builder's host's one-statement shims (its ops,
// the list's too, are Rust's: civ_host.rs). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <list>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/SESoundManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIFileListBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEARust.h"
#include "ee/EEGameSound.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEStringIDs.h"
#include "EEDataText.h"   // QuoteText: the keyed .civ text's name
#include "rust/game/screens.h"   // the builder (civ_whole.rs)
#include "ee/EEScreensShims.h"   // the host's shims' text arguments
#include "lle/UIFontTable.h"
#include "lle/GERectangle.h"
#include "lle/UILBString.h"
#include "lle/UIControlsTable.h"
#include <string.h>
#include <vector>
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "rust/sim/units.h"   // ee_units_lists_len
#include "rust/game/gamecpp.h"

// the civilization file being saved or loaded (EECBCivilizationFiles.cpp's file spec 0x565518: EELoadCivilization
// shares it)
extern FSFileSpec gU565518;

// ---- the entry points (the builder's Rust)

long EEProcessTimerExpired()
{
    return ee_screens_cb_timer_expired();
}


long CivBuilderSaveCivilization(const UString& inName, bool inOverwrite)
{
    return ee_screens_cb_save_civilization(reinterpret_cast<const uint8_t*>((const char*)inName), strlen(inName),
                                           inOverwrite);
}

void CivBuilderEditCurrentCivilization()
{
    ee_screens_cb_edit_current();
}


// the questions' Yes (DisplayQuestion's callbacks): the file written over, the selected file deleted, the changes
// accepted
static void EECBAnswerWrite(UIForm* inForm)
{
    ee_screens_cb_answer(EE_SCREENS_CBW_ANSWER_WRITE, inForm);
}

static void EECBAnswerDelete(UIForm* inForm)
{
    ee_screens_cb_answer(EE_SCREENS_CBW_ANSWER_DELETE, inForm);
}

static void EECBAnswerAccept(UIForm* inForm)
{
    ee_screens_cb_answer(EE_SCREENS_CBW_ANSWER_ACCEPT, inForm);
}

// ---- the host's shims (civ_host.rs: every op of the builder's host and of the list's in Rust; UI lane round 11)

static const EEUIFErrorCallback kCBAnswers[3] = {EECBAnswerWrite, EECBAnswerDelete, EECBAnswerAccept};

extern "C" uintptr_t ee_screenscpp_cbh_form(uint32_t inId) { return (uintptr_t)gUIManager->GetFormByID(inId); }
extern "C" uintptr_t ee_screenscpp_cbh_control_by_id(uintptr_t inForm, uint32_t inId)
{ return (uintptr_t)((UIForm*)inForm)->GetControlById(inId); }
extern "C" uintptr_t ee_screenscpp_cbh_control(uintptr_t inForm, uint32_t inIndex)
{ return (uintptr_t)((UIForm*)inForm)->GetControl(inIndex); }
extern "C" uintptr_t ee_screenscpp_cbh_as_edit(uintptr_t inControl)
{ return (uintptr_t)dynamic_cast<UIEditBox*>((UIControl*)inControl); }
extern "C" uintptr_t ee_screenscpp_cbh_as_label(uintptr_t inControl)
{ return (uintptr_t)dynamic_cast<UITextLabel*>((UIControl*)inControl); }
extern "C" uintptr_t ee_screenscpp_cbh_as_file_list(uintptr_t inControl)
{ return (uintptr_t)dynamic_cast<UIFileListBox*>((UIControl*)inControl); }
extern "C" uintptr_t ee_screenscpp_cbh_as_list(uintptr_t inControl)
{ return (uintptr_t)dynamic_cast<EECBUIListBox*>((UIControl*)inControl); }
extern "C" void ee_screenscpp_cbh_edit_text(uintptr_t inEdit, void* inOut)
{ EEShimOutC(inOut, ((UIEditBox*)inEdit)->GetText(0)); }
extern "C" void ee_screenscpp_cbh_edit_set_text(uintptr_t inEdit, EeScreensShimText inText)
{ ((UIEditBox*)inEdit)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_cbh_edit_select(uintptr_t inEdit) { ((UIEditBox*)inEdit)->SelectText(); }
extern "C" uint32_t ee_screenscpp_cbh_edit_index(uintptr_t inEdit) { return ((UIEditBox*)inEdit)->GetIndex(); }
extern "C" void ee_screenscpp_cbh_form_set_tab_stop(uintptr_t inForm, uint32_t inIndex)
{ ((UIForm*)inForm)->SetTabStop(inIndex); }
extern "C" void ee_screenscpp_cbh_label_text(uintptr_t inLabel, void* inOut)
{ EEShimOut(inOut, ((UITextLabel*)inLabel)->GetText(0)); }
extern "C" void ee_screenscpp_cbh_label_set_text(uintptr_t inLabel, EeScreensShimText inText)
{ ((UITextLabel*)inLabel)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_cbh_label_set_number(uintptr_t inLabel, int32_t inNumber)
{ ((UITextLabel*)inLabel)->SetText((long)inNumber); }
extern "C" int32_t ee_screenscpp_cbh_push(uintptr_t inForm) { return gUIManager->Push((UIForm*)inForm); }
extern "C" int32_t ee_screenscpp_cbh_close_form() { return gUserInterface->CloseForm(NULL); }
extern "C" uintptr_t ee_screenscpp_cbh_current_form() { return (uintptr_t)gUIManager->GetCurrentForm(); }
extern "C" void ee_screenscpp_cbh_path_new() { EEScratch<FSPath*>(kScratchCBPath) = new FSPath; }
extern "C" void ee_screenscpp_cbh_spec_volume() { EEScratch<FSFileSpec>(kScratchCBSpec).SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cbh_spec_directory()
{ EEScratch<FSFileSpec>(kScratchCBSpec).SetDirectory(gStartupDirectory + "Users\\default\\Civilizations\\"); }
extern "C" void ee_screenscpp_cbh_path_add() { EEScratch<FSPath*>(kScratchCBPath)->AddDirectory(EEScratch<FSFileSpec>(kScratchCBSpec), true); }
extern "C" void ee_screenscpp_cbh_fl_set_paths(uintptr_t inList) { ((UIFileListBox*)inList)->SetSearchPaths(EEScratch<FSPath*>(kScratchCBPath)); }
extern "C" void ee_screenscpp_cbh_fl_add_extension(uintptr_t inList)
{ ((UIFileListBox*)inList)->AddValidExtension(UString(".civ")); }
extern "C" void ee_screenscpp_cbh_fl_create_entries(uintptr_t inList) { ((UIFileListBox*)inList)->CreateEntries(1); }
extern "C" void ee_screenscpp_cbh_path_release() { EEScratch<FSPath*>(kScratchCBPath)->Dereference(); }
extern "C" int32_t ee_screenscpp_cbh_fl_selection(uintptr_t inList, unsigned long* inIndex)
{ return ((UIFileListBox*)inList)->GetCurrentSelection(*inIndex); }
extern "C" void ee_screenscpp_cbh_fl_line(uintptr_t inList, uint32_t inLine, void* inOut)
{ EEShimOut(inOut, ((UIFileListBox*)inList)->GetData(inLine)); }
extern "C" int32_t ee_screenscpp_cbh_fl_delete(uintptr_t inList)
{ return ((UIFileListBox*)inList)->DeleteSelectedFile(); }
extern "C" uintptr_t ee_screenscpp_cbh_screen_event(uint32_t inCode) { return (uintptr_t)EECreateScreenEvent(inCode); }
extern "C" void ee_screenscpp_cbh_form_set_open_event(uintptr_t inForm, uintptr_t inEvent)
{ ((UIForm*)inForm)->SetFormOpenEvent((UIEvent*)inEvent); }
extern "C" void ee_screenscpp_cbh_form_set_close_event(uintptr_t inForm, uintptr_t inEvent)
{ ((UIForm*)inForm)->SetFormCloseEvent((UIEvent*)inEvent); }
extern "C" void ee_screenscpp_cbh_event_release(uintptr_t inEvent) { ((UIEvent*)inEvent)->Dereference(); }
extern "C" uint32_t ee_screenscpp_cbh_form_id(uintptr_t inForm) { return ((UIForm*)inForm)->GetFormID(); }
extern "C" int32_t ee_screenscpp_cbh_string_load(uint32_t inId) { return gResource->GetString(inId, EEScratch<UWideString>(kScratchCBString)); }
extern "C" void ee_screenscpp_cbh_string_out(void* inOut) { EEShimOut(inOut, EEScratch<UWideString>(kScratchCBString)); }
extern "C" void ee_screenscpp_cbh_format_percent() { gResource->GetString(IDS_FORMAT_PERCENT_6_2G, EEScratch<UWideString>(kScratchCBFormat)); }
extern "C" void ee_screenscpp_cbh_format_reserved()
{ gResource->GetString(IDS_FILE_NAME_YOU_SPECIFIED_IS_RESERVED, EEScratch<UWideString>(kScratchCBFormat)); }
extern "C" void ee_screenscpp_cbh_print_float(float inValue) { sprintf(EEScratch<UWideString>(kScratchCBString), EEScratch<UWideString>(kScratchCBFormat), inValue); }
extern "C" void ee_screenscpp_cbh_print_text(EeScreensShimText inText)
{ sprintf(EEScratch<UWideString>(kScratchCBString), EEScratch<UWideString>(kScratchCBFormat), (const char*)EEShimString(inText)); }
extern "C" void ee_screenscpp_cbh_quote(EeScreensShimText inText, void* inOut)
{ EEShimOutC(inOut, EEDataText::QuoteText(std::string((const char*)inText.ptr, inText.len)).c_str()); }
extern "C" int32_t ee_screenscpp_cbh_civ_available(uintptr_t inCiv)
{ return ((EECivilization*)inCiv)->GetAvailablePoints(); }
extern "C" int32_t ee_screenscpp_cbh_civ_unchanged(uintptr_t inCiv) { return ((EECivilization*)inCiv)->GetUnchanged(); }
extern "C" int32_t ee_screenscpp_cbh_civ_max(uintptr_t inCiv) { return ((EECivilization*)inCiv)->GetMaxPoints(); }
extern "C" uint32_t ee_screenscpp_cbh_civ_purchased_count(uintptr_t inCiv)
{ return (uint32_t)((EECivilization*)inCiv)->GetPurchasedUpgrades().size(); }
extern "C" uint32_t ee_screenscpp_cbh_civ_purchased(uintptr_t inCiv, uint32_t inIndex)
{ return ((EECivilization*)inCiv)->GetPurchasedUpgrades()[inIndex]; }
extern "C" void ee_screenscpp_cbh_civ_name(uintptr_t inCiv, void* inOut)
{ EEShimOut(inOut, ((EECivilization*)inCiv)->mName); }
extern "C" void ee_screenscpp_cbh_upgrades_clear() { EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades).clear(); }
extern "C" void ee_screenscpp_cbh_upgrades_push(uint32_t inUpgrade) { EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades).push_back((EECivUpgrade)inUpgrade); }
extern "C" uint32_t ee_screenscpp_cbh_upgrades_count() { return (uint32_t)EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades).size(); }
extern "C" uint32_t ee_screenscpp_cbh_upgrades_at(uint32_t inIndex) { return EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades)[inIndex]; }
extern "C" uintptr_t ee_screenscpp_cbh_request_new(uint8_t inPlayer, EeScreensShimText inName)
{ return (uintptr_t) new EEMRecord(kMessageCivUpgradesRequested, {inPlayer, 0, EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades), EEShimText(inName)}); }
extern "C" bool ee_screenscpp_cbh_multiplayer() { return gCommunications->GetIsMultiplayerSession(); }
extern "C" void ee_screenscpp_cbh_send_server(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput((NEMessage*)inMessage, kSlotServer); }
extern "C" void ee_screenscpp_cbh_message_execute(uintptr_t inMessage) { ((NEMessage*)inMessage)->Execute(); }
extern "C" void ee_screenscpp_cbh_message_release(uintptr_t inMessage) { ((NEMessage*)inMessage)->Dereference(); }
extern "C" int32_t ee_screenscpp_cbh_game_result() { return gWorld.GetCurrentPlayer()->GetGameResult(); }
extern "C" int32_t ee_screenscpp_cbh_pause() { return gWorld.PauseGame(); }
extern "C" int32_t ee_screenscpp_cbh_unpause() { return gWorld.UnpauseGame(); }
extern "C" uintptr_t ee_screenscpp_cbh_current_player() { return (uintptr_t)gWorld.GetCurrentPlayer(); }
extern "C" uintptr_t ee_screenscpp_cbh_player_civ(uintptr_t inPlayer)
{ return (uintptr_t)&((EEPlayer*)inPlayer)->mCivilization; }
extern "C" uint8_t ee_screenscpp_cbh_player_id() { return (unsigned char)gWorld.mCurrentPlayer; }
extern "C" bool ee_screenscpp_cbh_civ_chosen() { return gWorld.GetCurrentPlayer()->mCivilizationChosen; }
extern "C" bool ee_screenscpp_cbh_civ_locked() { return gWorld.GetCurrentPlayer()->mCivilizationLocked; }
extern "C" void ee_screenscpp_cbh_set_civ_chosen(bool inValue)
{ gWorld.GetCurrentPlayer()->mCivilizationChosen = inValue; }
extern "C" void ee_screenscpp_cbh_set_civ_locked(bool inValue)
{ gWorld.GetCurrentPlayer()->mCivilizationLocked = inValue; }
extern "C" bool ee_screenscpp_cbh_free_camera() { return gFreeCamera; }
extern "C" bool ee_screenscpp_cbh_can_buy() { return gWorld.mGameOptions.CanBuyCivilization(); }
extern "C" bool ee_screenscpp_cbh_campaign_name() { return ee_gamecpp_options_campaign_map_name(&gWorld.mGameOptions, &(EEScratch<UWideString>(kScratchCBString))); }
extern "C" bool ee_screenscpp_cbh_custom_civs() { return gWorld.mGameOptions.mAllowCustomCivs; }
extern "C" uint32_t ee_screenscpp_cbh_sound_load(EeScreensShimText inName)
{ return (uint32_t)gSoundManager->Load(EEShimString(inName), 0, 1); }
extern "C" void ee_screenscpp_cbh_sound_unload(uint32_t inSound) { gSoundManager->Unload((unsigned long)inSound); }
extern "C" void ee_screenscpp_cbh_sound_play(uint32_t inSound)
{ gSoundManager->Play((unsigned long)inSound, 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" void ee_screenscpp_cbh_message(EeScreensShimText inText)
{ gUIManager->DisplayMessage(EEShimText(inText), 1, NULL, NULL, true); }
extern "C" void ee_screenscpp_cbh_question(EeScreensShimText inText, uintptr_t inForm, uint32_t inAnswer)
{ gUIManager->DisplayQuestion(EEShimText(inText), 1, (UIForm*)inForm, kCBAnswers[inAnswer], NULL); }
extern "C" void ee_screenscpp_cbh_set_blink_flag(bool inValue)
{ gUIManager->GetMainGameForm()->mIsButtonBlinking = inValue; }
extern "C" uintptr_t ee_screenscpp_cbh_main_builder_button(uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(dynamic_cast<UIBlinkingButton*>(((UIForm*)inForm)->GetControl(kMainGameCivSelectorBuilder))); }
extern "C" uintptr_t ee_screenscpp_cbh_main_alert_button()
{ return (uintptr_t)gUIManager->GetMainGameForm()->GetControl(kMainGameLargeCivBuilderAlert); }
extern "C" uint32_t ee_screenscpp_cbh_main_game_form_id() { return kFormMainGame; }
extern "C" void ee_screenscpp_cbh_button_blink(uintptr_t inButton, bool inValue)
{ static_cast<UIBlinkingButton*>((UIControl*)inButton)->SetIsBlinking(inValue); }
extern "C" void ee_screenscpp_cbh_f_volume() { gU565518.SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cbh_f_directory(EeScreensShimText inText) { gU565518.SetDirectory(EEShimString(inText)); }
extern "C" void ee_screenscpp_cbh_f_name(EeScreensShimText inText) { gU565518.SetFile(EEShimString(inText)); }
extern "C" int32_t ee_screenscpp_cbh_f_open_read() { return gU565518.Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_cbh_f_open_write() { return gU565518.Open(kFileAccessWrite); }
extern "C" int32_t ee_screenscpp_cbh_f_close() { return gU565518.Close(); }
extern "C" int32_t ee_screenscpp_cbh_f_delete() { return gU565518.Delete(); }
extern "C" int32_t ee_screenscpp_cbh_f_create() { return gU565518.Create(); }
extern "C" int32_t ee_screenscpp_cbh_f_write(EeScreensShimText inBytes)
{ return gU565518.Write((const char*)inBytes.ptr, (unsigned long)inBytes.len, EEScratch<unsigned long>(kScratchCBWritten)); }
extern "C" void ee_screenscpp_cbh_f_file(void* inOut) { EEShimOut(inOut, gU565518.GetFile()); }
extern "C" void ee_screenscpp_cbh_startup_directory(void* inOut) { EEShimOut(inOut, gStartupDirectory); }
extern "C" uint32_t ee_screenscpp_cbh_num_players() { return gWorld.mNumPlayers; }
extern "C" bool ee_screenscpp_cbh_player_civ_file_is(uint32_t inPlayer, EeScreensShimText inFile)
{ return gWorld.mPlayers[inPlayer]->mCivilizationFile + ".civ" == EEShimString(inFile); }
extern "C" void ee_screenscpp_cbh_players_clear() { EEScratch<std::vector<unsigned long>>(kScratchCBPlayers).clear(); }
extern "C" void ee_screenscpp_cbh_players_push(uint32_t inPlayer) { EEScratch<std::vector<unsigned long>>(kScratchCBPlayers).push_back(inPlayer); }
extern "C" uintptr_t ee_screenscpp_cbh_reset_players_new() { return (uintptr_t) new EEAResetAllPlayerInfo(EEScratch<std::vector<unsigned long>>(kScratchCBPlayers)); }
extern "C" void ee_screenscpp_cbh_post_action(uintptr_t inAction) { gUserInterface->PostAction((EEAction*)inAction); }
extern "C" void ee_screenscpp_cbh_action_release(uintptr_t inAction) { ((EEAction*)inAction)->Dereference(); }
extern "C" void ee_screenscpp_cbh_string_clear() { EEScratch<UWideString>(kScratchCBString) = UWideString(); }
extern "C" int32_t ee_screenscpp_cbh_load_civilization(EeScreensShimText inName, bool inUser)
{ return EELoadCivilization(EEShimString(inName), EEGetCivilizationDirectory(inUser), EEScratch<UWideString>(kScratchCBString), EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades)); }
extern "C" uint32_t ee_screenscpp_cbl_count(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetNumLines(); }
extern "C" void ee_screenscpp_cbl_line(uintptr_t inList, uint32_t inLine, void* inOut)
{ EEShimOut(inOut, ((EECBUIListBox*)inList)->mData[inLine]); }
extern "C" void ee_screenscpp_cbl_set_line(uintptr_t inList, uint32_t inLine, EeScreensShimText inText)
{ ((EECBUIListBox*)inList)->mData[inLine] = EEShimText(inText); }
extern "C" void ee_screenscpp_cbl_append(uintptr_t inList, EeScreensShimText inText)
{ ((EECBUIListBox*)inList)->UIListBox<UWideString>::AppendLine(EEShimText(inText)); }
extern "C" void ee_screenscpp_cbl_delete(uintptr_t inList, uint32_t inLine)
{ ((EECBUIListBox*)inList)->UIListBox<UWideString>::DeleteLine(inLine); }
extern "C" void ee_screenscpp_cbl_delete_all(uintptr_t inList) { ((EECBUIListBox*)inList)->DeleteAllEntries(); }
extern "C" void ee_screenscpp_cbl_select(uintptr_t inList, uint32_t inLine, bool inFire, bool inScroll)
{ ((EECBUIListBox*)inList)->UIListBox<UWideString>::ChangeSelectionIndex(inLine, inFire, inScroll); }
extern "C" int32_t ee_screenscpp_cbl_current(uintptr_t inList, unsigned long* inIndex)
{ return ((EECBUIListBox*)inList)->GetCurrentSelection(*inIndex); }
extern "C" bool ee_screenscpp_cbl_auto(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetAutoSelect(); }
extern "C" bool ee_screenscpp_cbl_has(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetHasSelection(); }
extern "C" uint32_t ee_screenscpp_cbl_index(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetSelectionIndex(); }
extern "C" void ee_screenscpp_cbl_dirty(uintptr_t inList) { ((EECBUIListBox*)inList)->SetDirty(true); }
extern "C" bool ee_screenscpp_cbl_upgrades_only(uintptr_t inList)
{ return ((EECBUIListBox*)inList)->mSelectUpgradesOnly; }
extern "C" bool ee_screenscpp_cbl_show_points(uintptr_t inList) { return ((EECBUIListBox*)inList)->mShowPoints; }
extern "C" bool ee_screenscpp_cbl_sort(uintptr_t inList) { return ((EECBUIListBox*)inList)->mSortLines; }
extern "C" bool ee_screenscpp_cbl_copied_points(uintptr_t inList)
{ return ((EECBUIListBox*)inList)->mCopiedShowPoints; }
extern "C" void ee_screenscpp_cbl_set_upgrades_only(uintptr_t inList, bool inValue)
{ ((EECBUIListBox*)inList)->mSelectUpgradesOnly = inValue; }
extern "C" void ee_screenscpp_cbl_set_show_points(uintptr_t inList, bool inValue)
{ ((EECBUIListBox*)inList)->mShowPoints = inValue; }
extern "C" void ee_screenscpp_cbl_set_sort(uintptr_t inList, bool inValue)
{ ((EECBUIListBox*)inList)->mSortLines = inValue; }
extern "C" void ee_screenscpp_cbl_set_copied_points(uintptr_t inList, bool inValue)
{ ((EECBUIListBox*)inList)->mCopiedShowPoints = inValue; }
extern "C" void ee_screenscpp_cbl_draw_base(uintptr_t inList) { ((EECBUIListBox*)inList)->UIControl::DrawTextA(); }
extern "C" uint32_t ee_screenscpp_cbl_draw_count(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetDrawCount(); }
extern "C" void ee_screenscpp_cbl_copy_line(uintptr_t inList, uint32_t inLine, void* inOut)
{ EEShimOut(inOut, ((EECBUIListBox*)inList)->mCopyLineArray[inLine]); }
extern "C" void ee_screenscpp_cbl_rect_copy(uintptr_t inList)
{ EEScratch<GERectangle>(kScratchCBRect) = ((EECBUIListBox*)inList)->GetCopyTextRectangle(); }
extern "C" float ee_screenscpp_cbl_rect_left() { return EEScratch<GERectangle>(kScratchCBRect).GetLeft(); }
extern "C" float ee_screenscpp_cbl_rect_right() { return EEScratch<GERectangle>(kScratchCBRect).GetRight(); }
extern "C" void ee_screenscpp_cbl_rect_set_left(float inValue) { EEScratch<GERectangle>(kScratchCBRect).SetLeft(inValue); }
extern "C" void ee_screenscpp_cbl_rect_set_right(float inValue) { EEScratch<GERectangle>(kScratchCBRect).SetRight(inValue); }
extern "C" void ee_screenscpp_cbl_rect_set_top(float inValue) { EEScratch<GERectangle>(kScratchCBRect).SetTop(inValue); }
extern "C" void ee_screenscpp_cbl_rect_set_bottom(float inValue) { EEScratch<GERectangle>(kScratchCBRect).SetBottom(inValue); }
extern "C" uint32_t ee_screenscpp_cbl_line_height(uintptr_t inList)
{ return ((EECBUIListBox*)inList)->GetLineHeight(); }
extern "C" float ee_screenscpp_cbl_copy_top(uintptr_t inList)
{ return ((EECBUIListBox*)inList)->GetCopyTextRectangle().GetTop(); }
extern "C" uintptr_t ee_screenscpp_cbl_text(uintptr_t inList)
{ return (uintptr_t)((EECBUIListBox*)inList)->GetGEText(); }
extern "C" void ee_screenscpp_cbl_font_bold(uintptr_t inText)
{ ((GEText*)inText)->SetFont(UIFontTable::FindOrCreateFont(((GEText*)inText)->GetFont()->GetFontName(), ((GEText*)inText)->GetFont()->GetFontStyle() | kFontStyleBold, ((GEText*)inText)->GetFont()->GetFontQuality(), ((GEText*)inText)->GetFont()->GetFontSize())); }
extern "C" void ee_screenscpp_cbl_font_plain(uintptr_t inText)
{ ((GEText*)inText)->SetFont(UIFontTable::FindOrCreateFont(((GEText*)inText)->GetFont()->GetFontName(), ((GEText*)inText)->GetFont()->GetFontStyle() & ~kFontStyleBold, ((GEText*)inText)->GetFont()->GetFontQuality(), ((GEText*)inText)->GetFont()->GetFontSize())); }
extern "C" void ee_screenscpp_cbl_align_right(uintptr_t inText) { ((GEText*)inText)->SetAlignment(kTextAlignRight); }
extern "C" void ee_screenscpp_cbl_align_left(uintptr_t inText) { ((GEText*)inText)->SetAlignment(kTextAlignLeft); }
extern "C" void ee_screenscpp_cbl_text_set(uintptr_t inText, EeScreensShimText inValue)
{ ((GEText*)inText)->SetText(EEShimText(inValue)); }
extern "C" void ee_screenscpp_cbl_colour_header(uintptr_t inText, uintptr_t inList)
{ ((GEText*)inText)->SetFgColor(((EECBUIListBox*)inList)->mC148); }
extern "C" void ee_screenscpp_cbl_colour(uintptr_t inText, float inRed, float inGreen, float inBlue)
{ ((GEText*)inText)->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_cbl_location_top_right(uintptr_t inText)
{ ((GEText*)inText)->SetLocation(EEScratch<GERectangle>(kScratchCBRect).GetTopRight()); }
extern "C" void ee_screenscpp_cbl_location_top_left(uintptr_t inText)
{ ((GEText*)inText)->SetLocation(EEScratch<GERectangle>(kScratchCBRect).GetTopLeft()); }
extern "C" int32_t ee_screenscpp_cbl_draw_clipped(uintptr_t inList, uintptr_t inText)
{ return ((EECBUIListBox*)inList)->GetForm()->DrawClippedText(EEScratch<GERectangle>(kScratchCBRect), (GEText*)inText, (EECBUIListBox*)inList); }
extern "C" int32_t ee_screenscpp_cbl_draw_end(uintptr_t inList)
{ return ((EECBUIListBox*)inList)->DrawScrollBarText(); }

// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.cpp (__FILE__ 0x27e426). The civilization builder's list
// box: 0x27cfdb..0x27efca. Its logic is Rust's (ee_screens civ_list.rs: AddLine, AddCategory, SortLines and the
// line order, ChangeSelectionIndex, SetUpgrades and FormatUpgrade, ReformatUpgrades, DeleteLine and its category's
// header, GetSelectedUpgrade, TabbedTo, DrawTextA's lines); this unit keeps the ctor, the snapshot and the
// forwarders (the base's calls Rust makes on a list are the builder host's shims: EECivilizationBuilder.cpp). Unit flags: the exe's module set.
// the scroll bar's opacity (a unit-local float ahead of the statics' strings in .rdata: 0x4cfa64; the statics, the
// separator "/" and the category "Category", had no reader)

// Creates a civilization builder list box (upgrade lines only selectable, points shown, sorted lines)
EECBUIListBox::EECBUIListBox(UIControlsTable& inTable, unsigned long inId, std::vector<UWideString>& inData,
                             GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                             unsigned long inBorderThickness, bool inIsTabable)
    : UILBString(inId, inData, inFont, true, inRectangle, inForm, inIndex, inBorderThickness)
{
    ee_screens_cbb_construct((uintptr_t)&inTable, (uintptr_t)static_cast<UIControl*>(this), inIsTabable);
}

// the constructor's body, AppendStringArray and CopyData: hud_bodies.rs over these (UI lane round 12)
#define CBB(l) static_cast<EECBUIListBox*>(reinterpret_cast<UIControl*>(l))
extern "C" void ee_screenscpp_cbb_setup_text(uintptr_t inTable, uintptr_t inList)
{ ((UIControlsTable*)inTable)->SetupGEText(CBB(inList)->GetGEText()); }
extern "C" void ee_screenscpp_cbb_set_header_colour(uintptr_t inList, float inRed, float inGreen, float inBlue)
{ CBB(inList)->mC13C = GEColor(inRed, inGreen, inBlue); }
extern "C" void ee_screenscpp_cbb_set_select_upgrades_only(uintptr_t inList, bool inValue)
{ CBB(inList)->mSelectUpgradesOnly = inValue; }
extern "C" void ee_screenscpp_cbb_set_show_points(uintptr_t inList, bool inValue)
{ CBB(inList)->mShowPoints = inValue; }
extern "C" void ee_screenscpp_cbb_set_tabable(uintptr_t inList, bool inValue) { CBB(inList)->mIsTabable = inValue; }
extern "C" void ee_screenscpp_cbb_scroll_bg_colour(uintptr_t inList, float inRed, float inGreen, float inBlue)
{ CBB(inList)->SetScrollBarBgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_cbb_scroll_bg_opacity(uintptr_t inList, float inOpacity)
{ CBB(inList)->SetScrollBarBgOpacity(inOpacity); }
extern "C" uintptr_t ee_screenscpp_cbb_lines_count(uintptr_t inLines)
{ return ((std::vector<UWideString>*)inLines)->size(); }
extern "C" void ee_screenscpp_cbb_line(uintptr_t inLines, uintptr_t inIndex, void* inOut)
{ EEShimOut(inOut, (*(std::vector<UWideString>*)inLines)[inIndex]); }
extern "C" int64_t ee_screenscpp_cbb_base_copy_data(uintptr_t inList)
{ return (int64_t)CBB(inList)->UILBString::CopyData(); }
extern "C" uint64_t ee_screenscpp_cbb_selection(uintptr_t inList) { return (uint64_t)CBB(inList)->GetSelectionIndex(); }
extern "C" void ee_screenscpp_cbb_copied_no_selection(uintptr_t inList) { CBB(inList)->SetCopiedHasSelection(false); }
extern "C" void ee_screenscpp_cbb_copy_header(uintptr_t inList) { CBB(inList)->mC148 = CBB(inList)->mC13C; }
extern "C" void ee_screenscpp_cbb_copy_show_points(uintptr_t inList)
{ CBB(inList)->mCopiedShowPoints = CBB(inList)->mShowPoints; }
extern "C" void ee_screenscpp_cbb_set_sort_lines(uintptr_t inList, bool inValue) { CBB(inList)->SetSortLines(inValue); }

// New (port), TEST ONLY: the civilization builder lane's capture records (tools/sim_capture.py).
// Reads the manager, the powers, the players and their class upgrades; changes nothing but the capture file (CVMG
// loads the premade civilizations' files through EELoadCivilization, as the game's choice form does). UI lane round
// 12: the records are Rust's (ee_game screens civ_capture.rs) over the one-statement shims below.
#define CP(p) ((EEPlayer*)(p))
#define CV(c) ((EECivilization*)(c))
// the unit types of a set, in its order
class EECivcCollect : public EEUnitSetFunctor {
public:
    virtual bool Execute(long inUnitType) { return EEScratch<std::vector<long>>(kScratchCivcTypes).push_back(inUnitType), true; }
};
// a temporary's address for the full expression (the set walker's functor argument)
template <class T> static T* EECivcTemp(T&& inValue) { return &inValue; }
extern "C" uintptr_t ee_screenscpp_civc_player(int64_t inIndex) { return (uintptr_t)::gWorld.mPlayers[inIndex]; }
extern "C" uintptr_t ee_screenscpp_civc_player_saved(uintptr_t inPlayer) { return (uintptr_t)static_cast<EeUnitsPlayerSaved*>(CP(inPlayer)); }
extern "C" uintptr_t ee_screenscpp_civc_player_fields(uintptr_t inPlayer) { return (uintptr_t)static_cast<EeUnitsPlayerFields*>(CP(inPlayer)); }
extern "C" void ee_screenscpp_civc_types_clear() { EEScratch<std::vector<long>>(kScratchCivcTypes).clear(); }
extern "C" void ee_screenscpp_civc_collect(int64_t inSet)
{ gUnitSetManager.ForEach((long)inSet, EECivcTemp(EECivcCollect())); }
extern "C" uintptr_t ee_screenscpp_civc_types_count() { return EEScratch<std::vector<long>>(kScratchCivcTypes).size(); }
extern "C" int64_t ee_screenscpp_civc_type_at(uintptr_t inIndex) { return (int64_t)EEScratch<std::vector<long>>(kScratchCivcTypes)[inIndex]; }
extern "C" uintptr_t ee_screenscpp_civc_class_upgrade(uintptr_t inPlayer, int64_t inKind)
{ return (uintptr_t)CP(inPlayer)->mUnitClassUpgrades[(long)inKind]; }
extern "C" uint64_t ee_screenscpp_civc_media_count(uintptr_t inPlayer)
{ return (uint64_t)ee_units_lists_len(CP(inPlayer)->mLists, EE_UNITS_LISTS_MEDIA); }
extern "C" int64_t ee_screenscpp_civc_civ_load(EeScreensShimText inFile)
{ return EELoadCivilization(EEShimString(inFile), EEGetCivilizationDirectory(false), EEScratch<UWideString>(kScratchCivcName), EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded)); }
extern "C" void ee_screenscpp_civc_loaded_clear() { EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded).clear(); }
extern "C" void ee_screenscpp_civc_name_clear() { EEScratch<UWideString>(kScratchCivcName) = UWideString(); }
extern "C" void ee_screenscpp_civc_name(void* inOut) { EEShimOut(inOut, EEScratch<UWideString>(kScratchCivcName)); }
extern "C" uint32_t ee_screenscpp_civc_name_length() { return (uint32_t)EEScratch<UWideString>(kScratchCivcName).GetLength(); }
extern "C" uintptr_t ee_screenscpp_civc_loaded_count() { return EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded).size(); }
extern "C" uint32_t ee_screenscpp_civc_loaded_at(uintptr_t inIndex) { return EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded)[inIndex]; }
extern "C" bool ee_screenscpp_civc_is_user_directory(EeScreensShimText inDirectory)
{ return EEShimString(inDirectory).Equals(EEGetCivilizationDirectory(true)); }
extern "C" uintptr_t ee_screenscpp_civc_civ(uintptr_t inPlayer) { return (uintptr_t)&CP(inPlayer)->mCivilization; }
extern "C" uintptr_t ee_screenscpp_civc_civ_purchased_count(uintptr_t inCiv)
{ return CV(inCiv)->mPurchasedUpgrades.size(); }
extern "C" uint32_t ee_screenscpp_civc_civ_purchased_at(uintptr_t inCiv, uintptr_t inIndex)
{ return CV(inCiv)->mPurchasedUpgrades[inIndex]; }
extern "C" uintptr_t ee_screenscpp_civc_civ_deferred_count(uintptr_t inCiv)
{ return CV(inCiv)->mDeferredUpgrades.size(); }
extern "C" uint32_t ee_screenscpp_civc_civ_deferred_at(uintptr_t inCiv, uintptr_t inIndex)
{ return CV(inCiv)->mDeferredUpgrades[inIndex]; }
extern "C" void ee_screenscpp_civc_civ_name(uintptr_t inCiv, void* inOut)
{ EEShimOutC(inOut, (const char*)CV(inCiv)->mName); }
extern "C" uint32_t ee_screenscpp_civc_civ_available(uintptr_t inCiv) { return (uint32_t)CV(inCiv)->mAvailablePoints; }
extern "C" uint32_t ee_screenscpp_civc_civ_max(uintptr_t inCiv) { return (uint32_t)CV(inCiv)->mMaxPoints; }
extern "C" bool ee_screenscpp_civc_civ_no_points(uintptr_t inCiv) { return CV(inCiv)->mNoPointsAdded; }
extern "C" bool ee_screenscpp_civc_civ_unchanged(uintptr_t inCiv) { return CV(inCiv)->mUnchanged; }
