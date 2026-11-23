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
#include "lle/User Interface.h"
#include "rust/ui/bgtableobj.h"

// Registers the user interface's shutdown hook (every background table deleted); always succeeds (ee_ui
// bgtableobj.rs). The original's unreferenced file-static UIBackgroundTable and its gUIInitialized flag (set here,
// read by nothing) are gone.
long UIInitialize() { return ee_ui_initialize(); }
