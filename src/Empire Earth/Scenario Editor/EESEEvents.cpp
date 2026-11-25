// C:\EEX\Empire Earth\Scenario Editor\EESEEvents.cpp (file name inferred: no __FILE__; class prefix EESEE). The
// scenario editor's UI events: 0x16f892..0x16ff5f. The trigger tab's events (0x16f892..0x16fd46: its buttons, the
// object, area, condition, effect and trigger screens' events) are EEScreenEvent codes handled by ee_screens
// scenario_editor.rs over this unit's host, ee_screenscpp_se (docs/RUST_PLAN.md "R6 game screens"); the events with
// per-instance state (the slider's label, the check box's controls, the scenario combo box's refill) are EESEERust
// events (ee_screens editor_events.rs, ee/EESEARust.h).
// The host also serves the editor form's and its tabs' events (the toolbar, the hot keys, the elevation tool, the
// player tab's epoch, the effects' radios, the instructions' "///" expansion: the units that newed them).
// After RTSPlayer.cpp (..0x16f892), before EESEIAExport.cpp's Virtual4 0x16ff5f. Unit flags: the exe module set.
#include <windows.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"

#include "lle/UITriStateCheckBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEARust.h"
#include "ee/EEUserInterface.h"
void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox);   // EESETSEffect.cpp

// ---- the host of the editor's events (ee_screens scenario_editor.rs): inForm is the scenario editor's form,
// inEntity the event's control. The toolkit's ops and the texts are answered in Rust (editor_direct.rs), the calls
// on the editor's Rust objects and the statement sequences composed there (editor_tabsx.rs, over
// ee_screenscpp_sex's single statements); these are the ops left.

// the exit question's answers (on the question's form)
static void EESESaveBeforeExit(UIForm* inForm) { ee_screens_event(EE_SCREENS_SE_SAVE_BEFORE_EXIT, inForm, NULL); }
static void EESEExitWithoutSaving(UIForm* inForm) { ee_screens_event(EE_SCREENS_SE_EXIT_WITHOUT_SAVING, inForm, NULL); }

extern "C" uint32_t ee_screenscpp_se(void* inForm, void* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB)
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(inForm));
    switch (inOp) {
    case EE_SCREENS_SE_OP_OPEN_CIV_BUILDER:
        return (uint32_t)gUserInterface->OpenCivilizationBuilder(&theEditor->mC1A0, true);
    case EE_SCREENS_SE_OP_IN_EDITOR:
        return gUIManager->GetInScenarioEditor() == true;
    }
    return 0;
}

namespace {

EEUIFScenarioEditor* SexEditor(void* const* p) { return static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[1])); }

}   // namespace

// the events' single statements (ee_screens editor_tabsx.rs EE_SCREENS_SEX_*: p0 the object, p1 the editor)
extern "C" int32_t ee_screenscpp_sex(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEX_DEBUG_IDS: return gWorld.mScenarioManager->mShowDebugIDs;
    case EE_SCREENS_SEX_SET_DEBUG_IDS: gWorld.mScenarioManager->mShowDebugIDs = i[0] != 0;
        return 0;
    case EE_SCREENS_SEX_NEW_CENTER: return EESEOut(i, static_cast<EEAction*>(new EEACenterCamera));
    case EE_SCREENS_SEX_CENTER_UNIT: static_cast<EEACenterCamera*>(static_cast<EEAction*>(p[0]))->SetUnit(SexEditor(p)->GetSelectedUnit((unsigned long)(uint32_t)i[0]));
        return 0;
    case EE_SCREENS_SEX_CENTER_LOCATION: static_cast<EEACenterCamera*>(static_cast<EEAction*>(p[0]))->SetLocation((long)i[0], (long)i[1]);
        return 0;
    case EE_SCREENS_SEX_PLACE_FILL: FillPlaceComboBox(i[0] != 0, static_cast<UITextLabel*>(SexEditor(p)->GetControlById(0x96d)), static_cast<UIComboBox*>(SexEditor(p)->GetControlById(0x999)));
        return 0;
    case EE_SCREENS_SEX_PLACE_FIRST: static_cast<UIComboBox*>(SexEditor(p)->GetControlById(0x999))->SetCurrentSelection(0);
        return 0;
    case EE_SCREENS_SEX_NEW_SAVE_ACTION: return EESEOut(i, static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_SAVE)));
    case EE_SCREENS_SEX_NEW_TEST_ACTION: return EESEOut(i, static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_TEST)));
    case EE_SCREENS_SEX_ASK_EXIT: gUIManager->DisplayYesNoCancel(gSefxText, 1, NULL, EESESaveBeforeExit, EESEExitWithoutSaving);
        return 0;
    case EE_SCREENS_SEX_CHECK_ID: return (int32_t)static_cast<UIControl*>(static_cast<UIEntity*>(p[0]))->GetId();
    case EE_SCREENS_SEX_CHECK_STATE: return (int32_t)static_cast<UITriStateCheckBox*>(static_cast<UIEntity*>(p[0]))->GetCheckState();
    }
    return 0;
}
