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
#include "platform/PlatMessage.h"
#include "ee/EEPhysics.h"
#include "rust/sim/tech.h"
#include "rust/game/screens.h"   // the host (civ_file_calls.rs)
#include "ee/EEScreensShims.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGameVariant.h"
#include "ee/EERulesWorld.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicEffect.h"
#include "rust/game/screens.h"   // the host and the premade list (civ_manager_calls.rs)
#include "ee/EEUnitUtilities.h"   // EEUpgradeHitPoints
#include "rust/game/civilization.h"
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

extern "C" uintptr_t ee_screenscpp_cbh_as_list(uintptr_t inControl)
{ return (uintptr_t)dynamic_cast<EECBUIListBox*>((UIControl*)inControl); }
extern "C" int32_t ee_screenscpp_cbh_close_form() { return gUserInterface->CloseForm(NULL); }
extern "C" void ee_screenscpp_cbh_path_new() { EEScratch<FSPath*>(kScratchCBPath) = new FSPath; }
extern "C" void ee_screenscpp_cbh_spec_volume() { EEScratch<FSFileSpec>(kScratchCBSpec).SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_cbh_spec_directory()
{ EEScratch<FSFileSpec>(kScratchCBSpec).SetDirectory(gStartupDirectory + "Users\\default\\Civilizations\\"); }
extern "C" void ee_screenscpp_cbh_path_add() { EEScratch<FSPath*>(kScratchCBPath)->AddDirectory(EEScratch<FSFileSpec>(kScratchCBSpec), true); }
extern "C" void ee_screenscpp_cbh_fl_set_paths(uintptr_t inList) { ((UIFileListBox*)inList)->SetSearchPaths(EEScratch<FSPath*>(kScratchCBPath)); }
extern "C" void ee_screenscpp_cbh_path_release() { EEScratch<FSPath*>(kScratchCBPath)->Dereference(); }
extern "C" uintptr_t ee_screenscpp_cbh_screen_event(uint32_t inCode) { return (uintptr_t)EECreateScreenEvent(inCode); }
extern "C" void ee_screenscpp_cbh_event_release(uintptr_t inEvent) { ((UIEvent*)inEvent)->Dereference(); }
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
extern "C" void ee_screenscpp_cbh_civ_name(uintptr_t inCiv, void* inOut)
{ EEShimOut(inOut, ((EECivilization*)inCiv)->mName); }
extern "C" void ee_screenscpp_cbh_upgrades_clear() { EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades).clear(); }
extern "C" void ee_screenscpp_cbh_upgrades_push(uint32_t inUpgrade) { EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades).push_back((EECivUpgrade)inUpgrade); }
extern "C" uint32_t ee_screenscpp_cbh_upgrades_count() { return (uint32_t)EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades).size(); }
extern "C" uint32_t ee_screenscpp_cbh_upgrades_at(uint32_t inIndex) { return EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades)[inIndex]; }
extern "C" uintptr_t ee_screenscpp_cbh_request_new(uint8_t inPlayer, EeScreensShimText inName)
{ return (uintptr_t) new EEMRecord(kMessageCivUpgradesRequested, {inPlayer, 0, EEScratch<std::vector<EECivUpgrade>>(kScratchCBUpgrades), EEShimText(inName)}); }
extern "C" void ee_screenscpp_cbh_send_server(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput((NEMessage*)inMessage, kSlotServer); }
extern "C" void ee_screenscpp_cbh_message_execute(uintptr_t inMessage) { ((NEMessage*)inMessage)->Execute(); }
extern "C" void ee_screenscpp_cbh_message_release(uintptr_t inMessage) { ((NEMessage*)inMessage)->Dereference(); }
extern "C" bool ee_screenscpp_cbh_campaign_name() { return ee_gamecpp_options_campaign_map_name(&gWorld.mGameOptions, &(EEScratch<UWideString>(kScratchCBString))); }
extern "C" void ee_screenscpp_cbh_message(EeScreensShimText inText)
{ gUIManager->DisplayMessage(EEShimText(inText), 1, NULL, NULL, true); }
extern "C" void ee_screenscpp_cbh_question(EeScreensShimText inText, uintptr_t inForm, uint32_t inAnswer)
{ gUIManager->DisplayQuestion(EEShimText(inText), 1, (UIForm*)inForm, kCBAnswers[inAnswer], NULL); }
extern "C" void ee_screenscpp_cbh_set_blink_flag(bool inValue)
{ gUIManager->GetMainGameForm()->mIsButtonBlinking = inValue; }
extern "C" uintptr_t ee_screenscpp_cbh_main_builder_button(uintptr_t inForm)
{ return (uintptr_t)static_cast<UIControl*>(dynamic_cast<UIBlinkingButton*>(((UIForm*)inForm)->GetControl(kMainGameCivSelectorBuilder))); }
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
extern "C" void ee_screenscpp_cbl_set_line(uintptr_t inList, uint32_t inLine, EeScreensShimText inText)
{ ((EECBUIListBox*)inList)->mData[inLine] = EEShimText(inText); }
extern "C" void ee_screenscpp_cbl_draw_base(uintptr_t inList) { ((EECBUIListBox*)inList)->UIControl::DrawTextA(); }
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
extern "C" void ee_screenscpp_cbl_colour(uintptr_t inText, float inRed, float inGreen, float inBlue)
{ ((GEText*)inText)->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_cbl_location_top_right(uintptr_t inText)
{ ((GEText*)inText)->SetLocation(EEScratch<GERectangle>(kScratchCBRect).GetTopRight()); }
extern "C" void ee_screenscpp_cbl_location_top_left(uintptr_t inText)
{ ((GEText*)inText)->SetLocation(EEScratch<GERectangle>(kScratchCBRect).GetTopLeft()); }
extern "C" int32_t ee_screenscpp_cbl_draw_clipped(uintptr_t inList, uintptr_t inText)
{ return ((EECBUIListBox*)inList)->GetForm()->DrawClippedText(EEScratch<GERectangle>(kScratchCBRect), (GEText*)inText, (EECBUIListBox*)inList); }

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
extern "C" uintptr_t ee_screenscpp_cbb_lines_count(uintptr_t inLines)
{ return ((std::vector<UWideString>*)inLines)->size(); }
extern "C" void ee_screenscpp_cbb_line(uintptr_t inLines, uintptr_t inIndex, void* inOut)
{ EEShimOut(inOut, (*(std::vector<UWideString>*)inLines)[inIndex]); }

// New (port), TEST ONLY: the civilization builder lane's capture records (tools/sim_capture.py).
// Reads the manager, the powers, the players and their class upgrades; changes nothing but the capture file (CVMG
// loads the premade civilizations' files through EELoadCivilization, as the game's choice form does). UI lane round
// 12: the records are Rust's (ee_game screens civ_capture.rs) over the one-statement shims below.
#define CP(p) ((EEPlayer*)(p))
#define CV(c) ((EECivilization*)(c))
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
extern "C" void ee_screenscpp_civc_civ_name(uintptr_t inCiv, void* inOut)
{ EEShimOutC(inOut, (const char*)CV(inCiv)->mName); }

// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationFiles.cpp (file name inferred: no __FILE__ or RTTI of its
// own). 0x27fdfe..0x2813f4, after EECivilizationBuilder.cpp and before EECivilizationManager.cpp. The builder's half
// (its state, the file being saved, the builder's open and close, buys, returns, deletes, accept: CivBuilder*) is
// Rust's (ee_screens civ_whole.rs); this unit keeps the simulation's: the civilization timer's limits, the
// civilizations' directory, a civilization file's load (EELoadCivilization: found and read here, its bytes read by
// ee_tech civ_file.rs), its application to a player (EEApplyCivilizationFile: the AI's, the campaign's, the player
// resets') and the server's grant of a player's upgrade request (EECBRequestUpgrades), both Rust's (civ_file.rs)
// over this unit's host (EECivFileCall). Unit flags: the exe module set.
// chain at 0x67fe94 (wrapper)
//   object 0x965518  ctor __imp_??0FSFileSpec@@QAE@XZ  dtor __imp_??1FSFileSpec@@UAE@XZ
// the civilization file being saved or loaded (the builder's host, EECivilizationBuilder.cpp, saves through it)
FSFileSpec gU565518;

// Returns the civilization choice time limit (300 s)
unsigned long EEGetCivTimerLimit()
{
    return 0x493e0;
}

// Returns the time of the civilization timer's first warning (240 s)
unsigned long EEGetCivTimerFirstWarning()
{
    return 0x3a980;
}

// Returns the time of the civilization timer's second warning (270 s)
unsigned long EEGetCivTimerSecondWarning()
{
    return 0x41eb0;
}

// Returns the civilization folder: the user's full path (Users\default\Civilizations\) or the relative one
UString EEGetCivilizationDirectory(bool inX)
{
    return inX ? gStartupDirectory + "Users\\default\\" + kCivilizationsKey + "\\" : kCivilizationsKey + "\\";
}

// Reads a civilization file's name and upgrade ids from the data path or the given directory, then the capture's
// record: Rust's (civ_load.rs over the shims below; ee_tech civ_file.rs reads the bytes: the legacy binary form or
// the keyed text)
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<EECivUpgrade>& outUpgrades)
{
    return ee_screens_civl_load((uintptr_t)&inName, (uintptr_t)&inDirectory, (uintptr_t)&outName, (uintptr_t)&outUpgrades);
}

// The C++ side of a file's application or a request (ee_tech civ_file.rs): the file's or the request's name, the
// upgrades, the player
struct EECivFileContext {
    UString* mFile;
    EEPlayer* mPlayer;
    unsigned char mPlayerIndex;
    UWideString mName;
    std::vector<EECivUpgrade> mUpgrades;
};

// the host: Rust's (civ_file_calls.rs) over the one-statement shims below (UI lane round 11)
static void EECivFileCall(void* inCtx, const EeTechCivFileCall* c, EeTechCivFileReply* r)
{
    ee_screens_civ_file_host(inCtx, c, r);
}

#define CF(x) static_cast<EECivFileContext*>((void*)(x))
extern "C" int64_t ee_screenscpp_civf_load(uintptr_t inCtx, bool inUser)
{ return EELoadCivilization(*CF(inCtx)->mFile, EEGetCivilizationDirectory(inUser), CF(inCtx)->mName, CF(inCtx)->mUpgrades); }
extern "C" uintptr_t ee_screenscpp_civf_name(uintptr_t inCtx) { return (uintptr_t)(const char*)CF(inCtx)->mName; }
extern "C" uint32_t ee_screenscpp_civf_name_characters(uintptr_t inCtx)
{ return (uint32_t)CF(inCtx)->mName.GetLength(); }
extern "C" uintptr_t ee_screenscpp_civf_upgrades(uintptr_t inCtx) { return (uintptr_t)CF(inCtx)->mUpgrades.data(); }
extern "C" uintptr_t ee_screenscpp_civf_upgrade_count(uintptr_t inCtx) { return CF(inCtx)->mUpgrades.size(); }
extern "C" void ee_screenscpp_civf_name_string(uintptr_t inCtx, int64_t inId)
{ gResource->GetString((long)inId, CF(inCtx)->mName); }
extern "C" void ee_screenscpp_civf_civ_clear(uintptr_t inCtx)
{ CF(inCtx)->mPlayer->mCivilization.mPurchasedUpgrades.clear(); }
extern "C" void ee_screenscpp_civf_civ_points_to_max(uintptr_t inCtx)
{ CF(inCtx)->mPlayer->mCivilization.mAvailablePoints = CF(inCtx)->mPlayer->mCivilization.mMaxPoints; }
extern "C" void ee_screenscpp_civf_civ_set_points_and_max(uintptr_t inCtx, uint64_t inPoints)
{ CF(inCtx)->mPlayer->mCivilization.SetPointsAndMax((unsigned long)inPoints); }
extern "C" void ee_screenscpp_civf_civ_set_name(uintptr_t inCtx)
{ CF(inCtx)->mPlayer->mCivilization.SetName(CF(inCtx)->mName); }
extern "C" void ee_screenscpp_civf_civ_can_afford(uintptr_t inCtx, uint32_t inUpgrade)
{ CF(inCtx)->mPlayer->mCivilization.CanAfford((EECivUpgrade)inUpgrade, &EEScratch<unsigned long>(kScratchCFCost), NULL); }
extern "C" uint64_t ee_screenscpp_civf_cost() { return (uint64_t)EEScratch<unsigned long>(kScratchCFCost); }
extern "C" uint64_t ee_screenscpp_civf_civ_max_points(uintptr_t inCtx)
{ return (uint64_t)CF(inCtx)->mPlayer->mCivilization.GetMaxPoints(); }
extern "C" void ee_screenscpp_civf_civ_buy(uintptr_t inCtx, uint32_t inUpgrade)
{ ee_gamecpp_civ_buy(&CF(inCtx)->mPlayer->mCivilization, (EECivUpgrade)inUpgrade, (long)(CF(inCtx)->mPlayer->mPlayerID), true); }
extern "C" void ee_screenscpp_civf_civ_set_points(uintptr_t inCtx) { CF(inCtx)->mPlayer->mCivilization.SetPoints(0); }
extern "C" bool ee_screenscpp_civf_purchased(uintptr_t inCtx) { return CF(inCtx)->mPlayer->mCivUpgradesPurchased; }
extern "C" bool ee_screenscpp_civf_can_afford_all(uintptr_t inCtx)
{ return ee_gamecpp_civ_can_afford_all(&CF(inCtx)->mPlayer->mCivilization, &(CF(inCtx)->mUpgrades)); }
extern "C" uintptr_t ee_screenscpp_civf_grant_new(uintptr_t inCtx, int64_t inPoints)
{ return (uintptr_t) new EEMRecord(kMessageCivUpgradesPurchased, {CF(inCtx)->mPlayerIndex, (long)inPoints, CF(inCtx)->mUpgrades, CF(inCtx)->mName}); }
extern "C" void ee_screenscpp_civf_send(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput((NEMessage*)inMessage, kSlotNone); }
extern "C" void ee_screenscpp_civf_execute(uintptr_t inMessage) { ((NEMessage*)inMessage)->Execute(); }
extern "C" void ee_screenscpp_civf_release(uintptr_t inMessage) { ((NEMessage*)inMessage)->Dereference(); }
extern "C" void ee_screenscpp_civf_refuse_format()
{ gResource->GetString(IDS_PLAYER_HAS_CIVILIZATION_NAMED_WHICH, EEScratch<UWideString>(kScratchCFFormat)); }
extern "C" void ee_screenscpp_civf_refuse_print(uintptr_t inCtx)
{ sprintf(EEScratch<UWideString>(kScratchCFText), EEScratch<UWideString>(kScratchCFFormat), (const char*)CF(inCtx)->mPlayer->mName, (const char*)CF(inCtx)->mName, (const char*)::gWorld.GetCurrentPlayer()->mName); }
extern "C" void ee_screenscpp_civf_chat_type()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSelectPlayerChatType, {0L}), kSlotNone); }
extern "C" void ee_screenscpp_civf_chat()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageInGameChat, {EEScratch<UWideString>(kScratchCFText)}), kSlotNone); }
extern "C" void ee_screenscpp_civf_random() { gPhysics->GetRandom(0L, 1L); }
extern "C" void ee_screenscpp_civf_strings_clear() { EEScratch<UWideString>(kScratchCFFormat) = UWideString(); }
extern "C" void ee_screenscpp_civf_text_clear() { EEScratch<UWideString>(kScratchCFText) = UWideString(); }

// Gives a player the civilization from a file (the game's, else the user's), buying its upgrades at no cost;
// a game file's premade civilization keeps its localized name (ee_tech civ_file.rs apply_file)
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer)
{
    UString theName = ioName;
    EECivFileContext theContext{&ioName, inPlayer, 0, UWideString(), {}};
    EeTechCivFileHost theHost{&theContext, &EECivFileCall};
    long theResult = ee_tech_civ_apply_file(&theHost, (const char*)ioName);
    ee_screens_civc_apply_file((const char*)theName, (uintptr_t)inPlayer, theResult);   // TEST ONLY: CVAF
    return theResult;
}

// Grants a player's civilization upgrade request when allowed, valid and affordable (broadcast in multiplayer);
// otherwise posts a chat line naming the refused civilization (ee_tech civ_file.rs request_upgrades)
void EECBRequestUpgrades(unsigned char inPlayer, UWideString& inName, std::vector<EECivUpgrade>& inUpgrades)
{
    EECivFileContext theContext{NULL, ::gWorld.mPlayers[inPlayer], inPlayer, inName, inUpgrades};
    EeTechCivFileHost theHost{&theContext, &EECivFileCall};
    ee_tech_civ_request_upgrades(&theHost);
}

// EELoadCivilization's file, read and results (civ_load.rs, UI lane round 12)
extern "C" void ee_screenscpp_civl_string(uintptr_t inString, void* inOut)
{ EEShimOut(inOut, *(const UString*)inString); }
extern "C" void ee_screenscpp_civl_global_close() { gU565518.Close(); }
extern "C" int32_t ee_screenscpp_civl_find(EeScreensShimText inPath, uintptr_t* inOut)
{ return gDataPath->FindFile(EEShimString(inPath), *reinterpret_cast<FSFileSpec**>(inOut)); }
extern "C" uintptr_t ee_screenscpp_civl_spec_new() { return (uintptr_t)new FSFileSpec; }
extern "C" void ee_screenscpp_civl_global_volume() { gU565518.SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_civl_global_directory(EeScreensShimText inDirectory)
{ gU565518.SetDirectory(EEShimString(inDirectory)); }
extern "C" void ee_screenscpp_civl_global_file(EeScreensShimText inFile) { gU565518.SetFile(EEShimString(inFile)); }
extern "C" void ee_screenscpp_civl_spec_assign_global(uintptr_t inSpec) { *(FSFileSpec*)inSpec = gU565518; }
extern "C" int32_t ee_screenscpp_civl_open(uintptr_t inSpec) { return ((FSFileSpec*)inSpec)->Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_civl_length(uintptr_t inSpec, unsigned long* inOut)
{ return ((FSFileSpec*)inSpec)->GetLength(*inOut); }
extern "C" void ee_screenscpp_civl_buffer_resize(uint64_t inSize) { EEScratch<std::vector<unsigned char>>(kScratchCivlBytes).resize((size_t)inSize); }
extern "C" int32_t ee_screenscpp_civl_read(uintptr_t inSpec, uint64_t inSize)
{ return ((FSFileSpec*)inSpec)->Read(EEScratch<std::vector<unsigned char>>(kScratchCivlBytes).data(), (unsigned long)inSize, EEScratch<unsigned long>(kScratchCivlGot)); }
extern "C" void ee_screenscpp_civl_buffer(void* inOut)
{ ee_screens_shim_out(inOut, EEScratch<std::vector<unsigned char>>(kScratchCivlBytes).data(), EEScratch<std::vector<unsigned char>>(kScratchCivlBytes).size()); }
extern "C" void ee_screenscpp_civl_set_name(uintptr_t inName, EeScreensShimText inText)
{ *(UWideString*)inName = EEShimText(inText); }
extern "C" void ee_screenscpp_civl_upgrade_push(uintptr_t inList, uint32_t inUpgrade)
{ ((std::vector<EECivUpgrade>*)inList)->push_back(inUpgrade); }
extern "C" void ee_screenscpp_civl_message(EeScreensShimText inText)
{ PlatMessageShow("Empire Earth: civilization", (const char*)EEShimString(inText), PLAT_MESSAGE_ERROR); }
extern "C" void ee_screenscpp_civl_close(uintptr_t inSpec) { ((FSFileSpec*)inSpec)->Close(); }
extern "C" void ee_screenscpp_civl_delete(uintptr_t inSpec) { delete (FSFileSpec*)inSpec; }
extern "C" void ee_screenscpp_civl_capture(uintptr_t inName, uintptr_t inDirectory, int64_t inResult, uintptr_t inOutName, uintptr_t inOutUpgrades)
{ ee_screens_civc_load(*(const UString*)inName, *(const UString*)inDirectory, (long)inResult, *(const UWideString*)inOutName, ((const std::vector<EECivUpgrade>*)inOutUpgrades)->data(), ((const std::vector<EECivUpgrade>*)inOutUpgrades)->size()); }

// C:\EEX\Empire Earth\Civilization Builder\EECivilizationManager.cpp (file name from the class; no __FILE__). The
// civilization categories, premade civilizations and upgrade ids (RTTI EECivilizationManager, vtable 0x44bfd4). Range
// 0x2813f4..0x2830d0; EECivPowersManager.cpp's 0x2830d0..0x283b2e folded in (civilization builder lane, round 2).
// Whole in Rust (ee_tech civ.rs: the tables and queries; civ_apply.rs: ApplyUpgrade, ApplyUpgradeLevel with
// EECBProcessUpgrade and EECBUpgradeArmor, the powers' ApplyPower): the two classes keep their names and methods as
// forwarders. UI lane round 12: their remaining bodies (the db files' reads, the premade copies, ApplyUpgrade with
// its world host, the out-parameter members) are ee_game screens civ_manager_obj.rs over the shims of this unit; the
// world calls are civ_manager_calls.rs'. Unit flags: the exe module set.
// chain at 0x681426 (wrapper)
//   object 0x9654c0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString kCivilizationsKey("Civilizations");

// Creates the civilization manager (the tables are Rust's: it binds the game's rules for them)
EECivilizationManager::EECivilizationManager()
{
    ee_tech_civ_bind_rules(EEGetRules().RustRules());
}

// ------------------------------------------------------------------------------------------------ the simulation's host

#define CIVH_P(p) (::gWorld.mPlayers[p])
#define CIVH_U(u) ((EEUnitClassUpgrade*)(u))
#define CIVH_K(k) ((EEUnitClass*)(k))
extern "C" void ee_screenscpp_civh_put_range(uintptr_t inUp, float inV) { CIVH_U(inUp)->SetRange(inV); }
extern "C" void ee_screenscpp_civh_put_max_velocity(uintptr_t inUp, float inV) { CIVH_U(inUp)->SetMaxVelocity(inV); }
extern "C" float ee_screenscpp_civh_variant_factor(int64_t inKind)
{ return ::gWorld.GetGameVariant()->GetFactor((long)inKind); }
extern "C" void ee_screenscpp_civh_hit_points(int32_t inPlayer, int64_t inKind, float inFactor)
{ EEUpgradeHitPoints(CIVH_P(inPlayer)->mUnitClassUpgrades[(long)inKind], (long)inKind, inFactor, CIVH_P(inPlayer)); }
extern "C" void ee_screenscpp_civh_sight_on(uintptr_t inUnit) { ((EEUnit*)inUnit)->SightOnHere(); }
extern "C" void ee_screenscpp_civh_sight_off(uintptr_t inUnit) { ((EEUnit*)inUnit)->SightOffHere(); }
extern "C" int64_t ee_screenscpp_civh_population_limit(int32_t inPlayer)
{ return CIVH_P(inPlayer)->GetPopulationLimit(0); }
extern "C" void ee_screenscpp_civh_recompute_area_effects(int32_t inPlayer)
{ CIVH_P(inPlayer)->SuggestRecomputeAllAreaEffects(); }
extern "C" void ee_screenscpp_civh_show_selection_areas(int32_t inPlayer)
{ ee_units_group_do(CIVH_P(inPlayer)->mSelectedGroup, 7, NULL); }
extern "C" uintptr_t ee_screenscpp_civh_sound(int64_t inKind) { return (uintptr_t)EESoundObject::Get((long)inKind); }
extern "C" void ee_screenscpp_civh_sound_load(uintptr_t inSound) { ((EESoundObject*)inSound)->LoadMedia(); }
extern "C" void ee_screenscpp_civh_add_sound(int32_t inPlayer, uintptr_t inSound)
{ CIVH_P(inPlayer)->AddLoadedMedia((EESoundObject*)inSound); }
extern "C" uintptr_t ee_screenscpp_civh_effect(int64_t inKind) { return (uintptr_t)EEGraphicEffect::Get((long)inKind); }
extern "C" void ee_screenscpp_civh_effect_load(uintptr_t inEffect) { ((EEGraphicEffect*)inEffect)->LoadMedia(); }
extern "C" void ee_screenscpp_civh_add_effect(int32_t inPlayer, uintptr_t inEffect)
{ CIVH_P(inPlayer)->AddLoadedMedia((EEGraphicEffect*)inEffect); }

// ------------------------------------------------------------------------------------------------ the powers

// the db files' reads, the premade copies and the powers' test: civ_manager_obj.rs over
// these (UI lane round 12)
extern "C" int32_t ee_screenscpp_civm_file_open(uintptr_t inFile) { return ((FSFile*)inFile)->Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_civm_read_chunk(uintptr_t inFile)
{ return ((FSFile*)inFile)->Read(EEScratch<unsigned char[0x1000]>(kScratchCivmChunk), sizeof(EEScratch<unsigned char[0x1000]>(kScratchCivmChunk)), EEScratch<unsigned long>(kScratchCivmRead)); }
extern "C" void ee_screenscpp_civm_chunk_bytes(void* inOut)
{ ee_screens_shim_out(inOut, EEScratch<unsigned char[0x1000]>(kScratchCivmChunk), sizeof(EEScratch<unsigned char[0x1000]>(kScratchCivmChunk))); }
extern "C" int32_t ee_screenscpp_civm_read_byte(uintptr_t inFile) { return ((FSFile*)inFile)->Read(EEScratch<unsigned char>(kScratchCivmByte)); }
extern "C" uint8_t ee_screenscpp_civm_byte() { return EEScratch<unsigned char>(kScratchCivmByte); }
extern "C" void ee_screenscpp_civm_file_close(uintptr_t inFile) { ((FSFile*)inFile)->Close(); }
extern "C" int64_t ee_screenscpp_civm_powers_cost()
{ return (int64_t)EERuleLong(EEGetRules().mCivBuilder.mPowersCost); }
extern "C" void ee_screenscpp_civm_civs_clear(uintptr_t inList)
{ ((std::vector<EECivilizationEntry>*)inList)->clear(); }
extern "C" void ee_screenscpp_civm_civs_push(uintptr_t inList, uintptr_t inEntry)
{ ((std::vector<EECivilizationEntry>*)inList)->push_back(*(const EECivilizationEntry*)inEntry); }

long EECivPowersManager::GetName(long inID, UWideString& outName)
{
    return gResource->GetString(inID + IDS_CIV_POWER_BASE, outName);
}
