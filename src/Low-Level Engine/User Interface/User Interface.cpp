// C:\EEX\Low-Level Engine\User Interface\User Interface.cpp (file name inferred, by analogy with
// Thread System\Thread System.cpp: the subsystem's initialize/shutdown entry points)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10082871..0x10082c43: the init chain of an unused file-static UIBackgroundTable, the gDbUI* file name chains, then
// UICreateMultilineTextFromString, UIGetStringCutOffIndex (now ee_ui cutoff.rs, its one caller the chat line's
// Rust), UIInitialize and its shutdown hook. UString.cpp starts at 0x10082c43.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/UIBackgroundTable.h"

// Constructed and destroyed, referenced by nothing (its chain comes first) (provisional name)
static UIBackgroundTable sBackgroundTable;

// the UI database file names (declared in lle/UIManager.h; UIManager::Create opens them)
LLE_API UString gDbUIBackgroundFileName("DB\\dbuiback.dat");
LLE_API UString gDbUIControlsFileName("DB\\dbuicontrols.dat");
LLE_API UString gDbUIFormsFileName("DB\\dbuiforms.dat");
LLE_API UString gDbUIControlEventsFileName("DB\\dbuicontrolevents.dat");
LLE_API UString gDbUIFormEventsFileName("DB\\dbuiformevents.dat");
LLE_API UString gDbUIFontsFileName("DB\\dbuifonts.dat");
LLE_API UString gDbUIHotKeyFileName("DB\\dbuihotkey.dat");

#include "lle/User Interface.h"

static void UIShutdown(bool inForced);

long gUIInitialized;

// Marks the user interface initialized and registers its shutdown hook; always succeeds
long UIInitialize()
{
    gUIInitialized = 1;
    URegisterShutdown(UIShutdown);
    return kErrorOK;
}

// Shutdown hook: deletes every background table
static void UIShutdown(bool inForced)
{
    UIBackgroundTable::DeleteAll();
}
