// C:\EEX\Empire Earth\UI Events\EEUIEScenarioIntroEvents.cpp (file name inferred: no __FILE__; class prefixes
// EEUIESIE, EEUISCIFormManager). The scenario introduction form's (0x13) form manager (its events are Rust's
// whole: ee_screens scenario_screens.rs, over UI Events/EEScreens.cpp):
// 0x1e71cc..0x1e853a (the save/load unit ends at 0x1e71cc; the scenario load unit starts at 0x1e853a). The form
// manager's members come first; its ??_G (0x1e83cb) sits in the ??_G block after the last event, so the manager is
// not a unit of its own (its only `new` is in Get). The ??_Gs of the classes this unit creates (Briefing, Movie,
// SoundOver, Start) follow in declaration order; deque<UIEventTrigger>::push_back (UIManager::AddEventToTrigger,
// SIE Open) closes the unit. Unit flags: the exe module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIFormManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUISCIFormManager.h"
// The manager's bodies are Rust's (UI lane round 9: ee_game screens sci_calls.rs) over the one-statement shims at
// the end of this file; each member is one forward
#include "ee/EEScreensShims.h"
#include "rust/game/screens.h"   // ee_screens_sci_* (sci_calls.rs)

// Resets the scenario intro manager: no open files and every flag cleared
void EEUISCIFormManager::Clear() { ee_screens_sci_clear((uintptr_t)this); }

// Closes and deletes the manager's map, scenario and sound files
void EEUISCIFormManager::DeleteObjects() { ee_screens_sci_delete_objects((uintptr_t)this); }

// Returns the scenario intro form's manager, creating and attaching it on first use
EEUISCIFormManager* EEUISCIFormManager::Get() { return reinterpret_cast<EEUISCIFormManager*>(ee_screens_sci_get()); }

// Returns the scenario's briefing text
UWideString EEUISCIFormManager::GetBriefing()
{
    return this->mMediaStrings.mInstructions;
}

// Opens the scenario's map picture (.jpg, else .png; the original's second choice was .tga) from Scenarios; returns
// whether one was found
bool EEUISCIFormManager::OpenMap() { return ee_screens_sci_open_map((uintptr_t)this); }

// Reopens the scenario file by name from Scenarios or the data path; returns whether it was found
bool EEUISCIFormManager::OpenScenario() { return ee_screens_sci_open_scenario((uintptr_t)this); }

// Opens the scenario's sound (.mp3) from Sounds; returns whether it was found
bool EEUISCIFormManager::OpenSound() { return ee_screens_sci_open_sound((uintptr_t)this); }

// Returns the scenario's history text
UWideString EEUISCIFormManager::GetHistory()
{
    return this->mMediaStrings.mHistory;
}

// Returns the scenario's hints text
UWideString EEUISCIFormManager::GetHints()
{
    return this->mMediaStrings.mHints;
}

// Opens the scenario's map picture, the scenario file or the sound and copies its file spec; returns whether it was
// found
bool EEUISCIFormManager::GetMap(FSFileSpec& outSpec) { return ee_screens_sci_get_map((uintptr_t)this, (uintptr_t)&outSpec); }
bool EEUISCIFormManager::GetScenario(FSFileSpec& outSpec)
{
    return ee_screens_sci_get_scenario((uintptr_t)this, (uintptr_t)&outSpec);
}
bool EEUISCIFormManager::GetSound(FSFileSpec& outSpec)
{
    return ee_screens_sci_get_sound((uintptr_t)this, (uintptr_t)&outSpec);
}

// Returns whether the scenario has any briefing, hints, history, map or sound (the original counted its movie too;
// no movie plays)
bool EEUISCIFormManager::HasMedia() { return ee_screens_sci_has_media((uintptr_t)this); }

// Starts or stops the main game's briefing button blinking, showing the large briefing alert while it blinks
void EEUISCIFormManager::SetBlinking(bool inBlinking) { ee_screens_sci_set_blinking(inBlinking); }

// ---- the shims (sci_calls.rs), each one statement on the manager (inM) or the main game form
#define SCI(inM) reinterpret_cast<EEUISCIFormManager*>(inM)
extern "C" void ee_screenscpp_sci_clear_loading(uintptr_t inM) { SCI(inM)->mIsLoadingFromGame = false; }
extern "C" void ee_screenscpp_sci_clear_in_game(uintptr_t inM) { SCI(inM)->mIsInGame = false; }
extern "C" void ee_screenscpp_sci_clear_ba0(uintptr_t inM) { SCI(inM)->mBA0 = false; }
extern "C" void ee_screenscpp_sci_clear_campaign(uintptr_t inM) { SCI(inM)->mIsCampaign = false; }
extern "C" void ee_screenscpp_sci_clear_scroll(uintptr_t inM) { SCI(inM)->mScrollBriefingToEnd = false; }
extern "C" uintptr_t ee_screenscpp_sci_map(uintptr_t inM) { return (uintptr_t)SCI(inM)->mMap; }
extern "C" uintptr_t ee_screenscpp_sci_file(uintptr_t inM) { return (uintptr_t)SCI(inM)->mFile; }
extern "C" uintptr_t ee_screenscpp_sci_sound(uintptr_t inM) { return (uintptr_t)SCI(inM)->mSound; }
extern "C" void ee_screenscpp_sci_delete_map(uintptr_t inM) { delete SCI(inM)->mMap; }
extern "C" void ee_screenscpp_sci_delete_file(uintptr_t inM) { delete SCI(inM)->mFile; }
extern "C" void ee_screenscpp_sci_delete_sound(uintptr_t inM) { delete SCI(inM)->mSound; }
extern "C" void ee_screenscpp_sci_map_null(uintptr_t inM) { SCI(inM)->mMap = NULL; }
extern "C" void ee_screenscpp_sci_file_null(uintptr_t inM) { SCI(inM)->mFile = NULL; }
extern "C" void ee_screenscpp_sci_sound_null(uintptr_t inM) { SCI(inM)->mSound = NULL; }
extern "C" uintptr_t ee_screenscpp_sci_form() { return (uintptr_t)gUIManager->GetFormByID(kFormScenarioIntro); }
extern "C" uintptr_t ee_screenscpp_sci_form_manager(uintptr_t inForm)
{ return (uintptr_t)static_cast<EEUISCIFormManager*>(reinterpret_cast<UIForm*>(inForm)->GetFormManager()); }
extern "C" uintptr_t ee_screenscpp_sci_new() { return (uintptr_t)new EEUISCIFormManager; }
extern "C" void ee_screenscpp_sci_form_set_manager(uintptr_t inForm, uintptr_t inM)
{ reinterpret_cast<UIForm*>(inForm)->SetFormManager(SCI(inM)); }
extern "C" void ee_screenscpp_sci_map_name(uintptr_t inM, void* inOut)
{ EEShimOut(inOut, SCI(inM)->MapFileName()); }
extern "C" void ee_screenscpp_sci_sound_name(uintptr_t inM, void* inOut)
{ EEShimOut(inOut, SCI(inM)->SoundFileName()); }
extern "C" void ee_screenscpp_sci_file_name(uintptr_t inM, void* inOut)
{ EEShimOut(inOut, SCI(inM)->mFile->GetFile()); }
extern "C" int64_t ee_screenscpp_sci_open_map(uintptr_t inM, EeScreensShimText inName)
{ return (int64_t)EEOpenSaveGameFile(EEShimString(inName), "Scenarios\\", SCI(inM)->mMap); }
extern "C" int64_t ee_screenscpp_sci_open_file(uintptr_t inM, EeScreensShimText inName)
{ return (int64_t)EEOpenSaveGameFile(EEShimString(inName), "Scenarios\\", SCI(inM)->mFile); }
extern "C" int64_t ee_screenscpp_sci_find_file(uintptr_t inM, EeScreensShimText inName)
{ return (int64_t)gDataPath->FindFile(EEShimString(inName), SCI(inM)->mFile); }
extern "C" int64_t ee_screenscpp_sci_open_sound(uintptr_t inM, EeScreensShimText inName)
{ return (int64_t)EEOpenSaveGameFile(EEShimString(inName), "Sounds\\", SCI(inM)->mSound); }
extern "C" void ee_screenscpp_sci_spec_from_map(uintptr_t inM, uintptr_t inSpec)
{ *reinterpret_cast<FSFileSpec*>(inSpec) = *SCI(inM)->mMap; }
extern "C" void ee_screenscpp_sci_spec_from_file(uintptr_t inM, uintptr_t inSpec)
{ *reinterpret_cast<FSFileSpec*>(inSpec) = *SCI(inM)->mFile; }
extern "C" void ee_screenscpp_sci_spec_from_sound(uintptr_t inM, uintptr_t inSpec)
{ *reinterpret_cast<FSFileSpec*>(inSpec) = *SCI(inM)->mSound; }
extern "C" bool ee_screenscpp_sci_has_instructions(uintptr_t inM)
{ return SCI(inM)->mMediaStrings.GetSection(3).GetLength() > 0; }
extern "C" bool ee_screenscpp_sci_has_hints(uintptr_t inM) { return SCI(inM)->mMediaStrings.GetSection(0).GetLength() > 0; }
extern "C" bool ee_screenscpp_sci_has_history(uintptr_t inM)
{ return SCI(inM)->mMediaStrings.GetSection(2).GetLength() > 0; }
extern "C" uintptr_t ee_screenscpp_sci_main_form() { return (uintptr_t)gUIManager->GetMainGameForm(); }
extern "C" uintptr_t ee_screenscpp_sci_main_control(uintptr_t inForm, uint32_t inControl)
{ return (uintptr_t)static_cast<UIBlinkingButton*>(reinterpret_cast<EEUIFMainGame*>(inForm)->GetControl(inControl)); }
extern "C" void ee_screenscpp_sci_set_info_blinking(uintptr_t inForm, bool inBlinking)
{ reinterpret_cast<EEUIFMainGame*>(inForm)->mInfoBlinking = inBlinking; }
extern "C" void ee_screenscpp_sci_button_blinking(uintptr_t inButton, bool inBlinking)
{ reinterpret_cast<UIBlinkingButton*>(inButton)->SetIsBlinking(inBlinking); }
extern "C" void ee_screenscpp_sci_button_shown(uintptr_t inButton)
{ reinterpret_cast<UIBlinkingButton*>(inButton)->SetDisplayMode(kDisplayModeShown); }
extern "C" void ee_screenscpp_sci_button_hidden(uintptr_t inButton)
{ reinterpret_cast<UIBlinkingButton*>(inButton)->SetDisplayMode(kDisplayModeHidden); }
