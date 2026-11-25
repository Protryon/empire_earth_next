// C:\EEX\Empire Earth\Scenario Editor\EESEIAExport.cpp (file name inferred: no __FILE__; RTTI EESEIAExport). The trigger
// text export/import actions: 0x16ff5f..0x1705a2 (EESEIAExport::Virtual4, the export question's answer, EESEIAExport's
// ??_G, EESEIEExport::ProcessEvent, EESEIAImport::Virtual4). After EESEEvents.cpp (0x16f892..0x16ff5f), before
// EESEInstructions.cpp (0x1705a2). Unit flags: the exe module set. The bodies are Rust's (ee_game screens
// editor_selt.rs; their statements are EESELoadThread.cpp's ee_screenscpp_seltx); only the answers' callbacks stay.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EEUIManager.h"
// the questions' answers (the callbacks DisplayQuestion takes): the export of the spec, the import of the info
// (ee_screens editor_selt.rs; the actions and the buttons' events are EESEARust and EESEERust kinds since round 6)
void EESEExportTextAnswer(UIForm* inFile)
{
    ee_screens_seia_answer(inFile, false);
}

void EESEImportTextAnswer(UIForm* inInfo)
{
    ee_screens_seia_answer(inInfo, true);
}
