// C:\EEX\Empire Earth\Scenario Editor\EESEIAExport.cpp (file name inferred: no __FILE__; RTTI EESEIAExport). The trigger
// text export/import actions: 0x16ff5f..0x1705a2 (EESEIAExport::Virtual4, the export question's answer, EESEIAExport's
// ??_G, EESEIEExport::ProcessEvent, EESEIAImport::Virtual4). After EESEEvents.cpp (0x16f892..0x16ff5f), before
// EESEInstructions.cpp (0x1705a2). Unit flags: the exe module set. The bodies are Rust's (ee_game screens
// editor_selt.rs; their statements are EESELoadThread.cpp's ee_screenscpp_seltx); only the answers' callbacks stay.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"

#include "ee/EEStringIDs.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "lle/USingleton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "rust/game/screens.h"
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
