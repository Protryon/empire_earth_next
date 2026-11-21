// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEMap.cpp (__FILE__ 0x4c5148, the ctor's ESVERIFYs). The editor's map tab:
// 0x1a18f1..0x1a3f10 (UIBMap::ButtonUp, UIRBMap's Check/HideControls, EEUIFSEEMapType::ProcessEvent, the tab's ctor,
// the helper classes' ??_G, the tab's ??_G/dtor/members, then the vector push_back and EETerrainTable::GetNumTables
// COMDATs). After EEUIFSEMainButton.cpp (0x1a147f), before EEUIFSEPaint.cpp (0x1a3f10). Unit flags: the exe module
// set.
//
// The tab whole in Rust (ee_screens editor_map.rs; its group editor_groups_ffi.rs holds it: the class EEUIFSEMap is
// gone). This unit is the tab's host
// (ee_screenscpp_semp): the controls only the tab makes and the world it reads and posts to.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UIRadioButton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUserInterface.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUIShell.h"
#include "ee/EESEARust.h"
#include "ee/EEStringIDs.h"
#include "ee/EERules.h"
#include "rust/game/screens.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"

void SeplLinePush(const UWideString& inLine);   // a line for the next combo box made with pending lines (the same)

// ---- the tab's host (editor_map.rs: the ops from 500)



namespace {


}   // namespace

extern "C" int32_t ee_screenscpp_semp(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEMP_OP_TERRAIN_COUNT: return (int32_t)EETerrainTable::GetNumTables();
    }
    return 0;
}

// the tab's single statements (ee_screens editor_tabsx.rs EE_SCREENS_SEMPX_*: the Rust composes the host's ops)
extern "C" int32_t ee_screenscpp_sempx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEMPX_TERRAIN_EXISTS: return EETerrainTable::Get((unsigned long)i[0]) != NULL;
    case EE_SCREENS_SEMPX_TERRAIN_IN_EDITOR: return EETerrainTable::Get((unsigned long)i[0])->mIsInEditor;
    case EE_SCREENS_SEMPX_TERRAIN_ID: return (int32_t)EETerrainTable::Get((unsigned long)i[0])->mID;
    case EE_SCREENS_SEMPX_TERRAIN_NAME: return (int32_t)EETerrainTable::Get((unsigned long)i[0])->mNameID;
    case EE_SCREENS_SEMPX_GENERATORS: return (int32_t)EERMGetNumberOfGenerators();
    case EE_SCREENS_SEMPX_PUSH_GENERATOR: SeplLinePush(EERMGetGeneratorDisplayName((unsigned long)i[0]));
        return 0;
    case EE_SCREENS_SEMPX_MAP_SIZES: return (int32_t)EEGetRules().mGameOptions.mMapSizes.size();
    case EE_SCREENS_SEMPX_MAP_SIZE_NAME: gResource->GetString(EEGetRules().mGameOptions.mMapSizes[(size_t)i[0]].mNameID, gSefxText);
        return 0;
    case EE_SCREENS_SEMPX_PUSH_TEXT: SeplLinePush(gSefxText);
        return 0;
    case EE_SCREENS_SEMPX_RADIO_HEIGHT: return (int32_t)(unsigned long)theForm->GetManager()->GetRadioOn()->GetHeight();
    case EE_SCREENS_SEMPX_FONT_HEIGHT: return (int32_t)(unsigned long)gSeplScreenFont->GetHeight();
    case EE_SCREENS_SEMPX_NEW_RADIO: return EESEOut(i, static_cast<UIControl*>(new EEUIShellRadio(EE_SCREENS_SK_MAP_RADIO, {}, (unsigned long)i[0], gSefxText, gSeplScreenFont, GERectangle(GE2DPoint(f[0], f[1]), 100.0f, (float)(unsigned long)f[2]), theForm, (unsigned long)i[2], 0, false, i[3] != 0, false)));
    case EE_SCREENS_SEMPX_RADIO_LABEL: return EESEOut(i, static_cast<UIControl*>(static_cast<UIRadioButton*>(static_cast<UIControl*>(p[0]))->GetTextLabel()));
    case EE_SCREENS_SEMPX_NEW_BUTTON: return EESEOut(i, static_cast<UIControl*>(new EEUIShellButton(EE_SCREENS_SK_MAP_BUTTON, {(int64_t)p[2]}, 0x65, NULL, NULL, NULL, GERectangle(GE2DPoint(f[0], f[1]), 260.0f, (float)(unsigned long)(gSeplScreenFont->GetHeight() + 10.0f)), theForm, (unsigned long)i[2], 2)));
    case EE_SCREENS_SEMPX_BUTTON_TEXT: static_cast<UIButton*>(static_cast<UIControl*>(p[0]))->SetText(gSefxText, gSeplScreenFont);
        return 0;
    case EE_SCREENS_SEMPX_ELEVATION_DIRECTORY: static_cast<FSFileSpec*>(p[0])->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
        return 0;
    case EE_SCREENS_SEMPX_NEW_PATH: return EESEOut(i, new FSPath);
    case EE_SCREENS_SEMPX_PATH_ADD: static_cast<FSPath*>(p[0])->AddDirectory(*static_cast<FSFileSpec*>(p[2]), true);
        return 0;
    case EE_SCREENS_SEMPX_PATH_RELEASE: static_cast<FSPath*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEMPX_NEW_FILE_COMBO: return EESEOut(i, static_cast<UIControl*>(EEUIShellNewFileCombo(static_cast<FSPath*>(p[0]), UString("*") + ".dat", EE_SCREENS_SE_MAP_ELEVATION_FILE, gSeplScreenFont, GE2DPoint(f[0], f[1]), 200, theForm, (unsigned long)i[2])));
    case EE_SCREENS_SEMPX_FILE_COMBO_HELP: static_cast<UIControl*>(p[0])->SetStringTableID(IDS_ELEVATION_FILE_TO_USE_WITH_BLANK);
        return 0;
    case EE_SCREENS_SEMPX_BLANK_MAP: (new EESEARust(kSEABlankMap, NULL, i[0], i[1], i[2]))->Dereference();
        return 0;
    case EE_SCREENS_SEMPX_RANDOM_MAP: (new EESEARust(kSEARandomMap, NULL, i[0], i[1]))->Dereference();
        return 0;
    case EE_SCREENS_SEMPX_RANDOM_NEW: return EESEOut(i, new URandomIntegers);
    case EE_SCREENS_SEMPX_RANDOM_SEED: static_cast<URandomIntegers*>(p[0])->SetSeed(TMGetTime());
        return 0;
    case EE_SCREENS_SEMPX_RANDOM_DRAW: return (int32_t)static_cast<URandomIntegers*>(p[0])->Generate(0, 1000000);
    case EE_SCREENS_SEMPX_RANDOM_DELETE: delete static_cast<URandomIntegers*>(p[0]);
        return 0;
    case EE_SCREENS_SEMPX_SEED_SET: gUserInterface->mRandomSeed = i[0];
        return 0;
    case EE_SCREENS_SEMPX_SEED: return (int32_t)gUserInterface->mRandomSeed;
    case EE_SCREENS_SEMPX_PROCESS_SELECTION: static_cast<UIControl*>(p[0])->GetTriggerEvent(kControlTriggerSelectionChanged)->ProcessEvent(NULL, static_cast<UIControl*>(p[0]));
        return 0;
    case EE_SCREENS_SEMPX_FILE_ENTRIES: EEUIFileComboEntries(static_cast<UIControl*>(p[0]));
        return 0;
    }
    return 0;
}
