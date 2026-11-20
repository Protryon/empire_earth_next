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
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"

#include "lle/UICheckBox.h"
#include "lle/UITriStateCheckBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEARust.h"
#include "ee/EEUIShell.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"
#include "ee/EEUserInterface.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESEUnit.h"
#include "ee/EESEARust.h"
void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox);   // EESETSEffect.cpp

// ---- the host of the editor's events (ee_screens scenario_editor.rs): inForm is the scenario editor's form,
// inEntity the event's control. The editor's objects, one call each (EE_SCREENS_SE_OP_*); the toolkit's ops and the
// texts are answered in Rust (editor_direct.rs).
static GE2DPoint sAreaCenter;   // EE_SCREENS_SE_OP_AREA_CENTER's, for EE_SCREENS_SE_OP_CENTER_ON_AREA

// the exit question's answers (on the question's form)
static void EESESaveBeforeExit(UIForm* inForm) { ee_screens_event(EE_SCREENS_SE_SAVE_BEFORE_EXIT, inForm, NULL); }
static void EESEExitWithoutSaving(UIForm* inForm) { ee_screens_event(EE_SCREENS_SE_EXIT_WITHOUT_SAVING, inForm, NULL); }

static void EESEPostAction(EEAction* inAction)
{
    gUserInterface->PostAction(inAction);
    inAction->Dereference();
}

extern "C" uint32_t ee_screenscpp_se(void* inForm, void* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB)
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(inForm));
    UIEntity* theEntity = static_cast<UIEntity*>(inEntity);
    // a sub-screen's own call with its selection where its class passed it
    auto Sub = [&](long inScreen, uint32_t inWhat, uint32_t inA = 0, uint32_t inB = 0) {
        return theEditor->TriggerSubOp(inScreen, inWhat, inA, inB);
    };
    auto Object = [&](uint32_t inWhat, uint32_t inA = 0, uint32_t inB = 0) {
        return Sub(kSETriggerScreenObject, inWhat, inA, inB);
    };
    auto Trigger = [&](uint32_t inWhat, uint32_t inA = 0) { return Sub(kSETriggerScreenTrigger, inWhat, inA); };
    switch (inOp) {
    case EE_SCREENS_SE_OP_TAB: {
        switch (inA) {
        case EE_SCREENS_SE_TAB_SHOW: theEditor->TriggerCall(EE_SCREENS_SETS_SHOW_TAB, inB); break;
        case EE_SCREENS_SE_TAB_SELECT_ENTRY: theEditor->TriggerCall(EE_SCREENS_SETS_SELECT_ENTRY); break;
        case EE_SCREENS_SE_TAB_NEW_ENTRY: theEditor->TriggerCall(EE_SCREENS_SETS_NEW_ENTRY); break;
        case EE_SCREENS_SE_TAB_DUPLICATE_ENTRY: theEditor->TriggerCall(EE_SCREENS_SETS_DUPLICATE_ENTRY); break;
        case EE_SCREENS_SE_TAB_DELETE_ENTRY: theEditor->TriggerCall(EE_SCREENS_SETS_DELETE_ENTRY); break;
        case EE_SCREENS_SE_TAB_INC_PRIORITY: theEditor->TriggerCall(EE_SCREENS_SETS_INC_PRIORITY); break;
        case EE_SCREENS_SE_TAB_DEC_PRIORITY: theEditor->TriggerCall(EE_SCREENS_SETS_DEC_PRIORITY); break;
        case EE_SCREENS_SE_TAB_ALPHA_SORT: theEditor->TriggerCall(EE_SCREENS_SETS_ALPHA_SORT); break;
        default: theEditor->TriggerCall(EE_SCREENS_SETS_SELECT_NAME); break;
        }
        break;
    }
    case EE_SCREENS_SE_OP_TOGGLE_DEBUG_IDS:
        gWorld.mScenarioManager->mShowDebugIDs = !gWorld.mScenarioManager->mShowDebugIDs;
        break;
    case EE_SCREENS_SE_OP_OBJECT_SET_MAP:
        Object(EE_SCREENS_SEOB_SET_SELECT_ON_MAP, inA != 0);
        break;
    case EE_SCREENS_SE_OP_OBJECT_REFRESH:
        Object(EE_SCREENS_SEOB_REFRESH);
        break;
    case EE_SCREENS_SE_OP_OBJECT_IS_MAP:
        return Object(EE_SCREENS_SEOB_IS_SELECT_ON_MAP) != 0;
    case EE_SCREENS_SE_OP_MANAGER_CLEAR_SELECTION:
        gUIManager->GetScenarioEditorForm()->ClearSelection();
        break;
    case EE_SCREENS_SE_OP_OBJECT_CLEAR_UNIT_LIST:
        Object(EE_SCREENS_SEOB_CLEAR_UNIT_LIST);
        break;
    case EE_SCREENS_SE_OP_FORM_CLEAR_SELECTION:
        theEditor->ClearSelection();
        break;
    case EE_SCREENS_SE_OP_OBJECT_CLEAR_IDS:
        Object(EE_SCREENS_SEOB_CLEAR_UNIT_IDS);
        break;
    case EE_SCREENS_SE_OP_OBJECT_UPDATE_SELECTION:
        Object(EE_SCREENS_SEOB_UPDATE_SELECTION);
        break;
    case EE_SCREENS_SE_OP_OBJECT_CHECK_CHANGED:
        Object(EE_SCREENS_SEOB_CHECK_CHANGED, (uint32_t) static_cast<UIControl*>(theEntity)->GetId(),
               (uint32_t) static_cast<UITriStateCheckBox*>(theEntity)->GetCheckState());
        break;
    case EE_SCREENS_SE_OP_SELECTED_UNIT_COUNT:
        return theEditor->GetSelectedUnitCount();
    case EE_SCREENS_SE_OP_CENTER_ON_UNIT: {
        EEACenterCamera* theAction = new EEACenterCamera;
        theAction->SetUnit(theEditor->GetSelectedUnit(inA));
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_SE_OP_AREA_MODE:
        Sub(kSETriggerScreenArea, EE_SCREENS_SEAR_SET_MODE, inA != 0 ? 1 : 0);
        break;
    case EE_SCREENS_SE_OP_AREA_CENTER: {
        float theCenter[2];
        if (!ee_screens_sear_center(static_cast<EeScreensSeArea*>(theEditor->GetTriggerSub(kSETriggerScreenArea)),
                                    theCenter))
            return 0;
        sAreaCenter = GE2DPoint(theCenter[0], theCenter[1]);
        return 1;
    }
    case EE_SCREENS_SE_OP_CENTER_ON_AREA: {
        EEACenterCamera* theAction = new EEACenterCamera;
        theAction->SetLocation((long)sAreaCenter.GetX(), (long)sAreaCenter.GetY());
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_SE_OP_CONDITION_TYPE:
        Sub(kSETriggerScreenCondition, EE_SCREENS_SESC_CHANGE_TYPE);
        break;
    case EE_SCREENS_SE_OP_EFFECT_TYPE:
        Sub(kSETriggerScreenEffect, EE_SCREENS_SESC_CHANGE_TYPE);
        break;
    case EE_SCREENS_SE_OP_EFFECT_ACTION:
        Sub(kSETriggerScreenEffect, EE_SCREENS_SESC_CHANGE_ACTION);
        break;
    case EE_SCREENS_SE_OP_TRIGGER_OPERATOR:
        Trigger(EE_SCREENS_SETR_CHANGE_OPERATOR, inA);
        break;
    case EE_SCREENS_SE_OP_TRIGGER_STEP:
        Trigger(EE_SCREENS_SETR_STEP, inA < 3 ? inA : 3);
        break;
    case EE_SCREENS_SE_OP_TRIGGER_REMOVE:
        Trigger(EE_SCREENS_SETR_REMOVE_ENTRY, inA);
        break;
    // the editor form's and its tabs' (group 2)
    case EE_SCREENS_SE_OP_UNIT_RESELECT:
        theEditor->UnitCall(EE_SCREENS_SEUN_RESELECT);
        break;
    case EE_SCREENS_SE_OP_PLAYER_FILL_TECH:
        theEditor->PlayerCall(EE_SCREENS_SEPL_FILL_TECH_LISTS);
        break;
    case EE_SCREENS_SE_OP_HOME_ROTATION: {
        GETransformation theTransformation;
        theTransformation.MakeIdentity();
        gWorld.mCameraControl.SetViewTurn(theTransformation, false);
        theTransformation.SetOrientationYPR(0.7853982f, -0.61086524f, 0.0f);
        gWorld.mCameraControl.SetOffsetTurn(theTransformation, false);
        break;
    }
    case EE_SCREENS_SE_OP_OPEN_CIV_BUILDER:
        return (uint32_t)gUserInterface->OpenCivilizationBuilder(&theEditor->mC1A0, true);
    case EE_SCREENS_SE_OP_LOAD_TRIGGERS: {
        FSFileSpec theFile;
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory);
        theFile.SetFile("ScenarioTriggers.scn");
        if (theFile.Open(kFileAccessRead) == kErrorOK)
            gWorld.mScenarioManager->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
        theFile.Close();
        break;
    }
    case EE_SCREENS_SE_OP_PLAYER_REBUILD_TECH:
        theEditor->PlayerCall(EE_SCREENS_SEPL_REBUILD_TECH_LISTS);
        break;
    case EE_SCREENS_SE_OP_PLACE_FILL: {
        UIComboBox* theComboBox = static_cast<UIComboBox*>(theEditor->GetControlById(0x999));
        FillPlaceComboBox(inA != 0, static_cast<UITextLabel*>(theEditor->GetControlById(0x96d)), theComboBox);
        theComboBox->SetCurrentSelection(0);
        break;
    }
    case EE_SCREENS_SE_OP_IN_EDITOR:
        return gUIManager->GetInScenarioEditor() == true;
    case EE_SCREENS_SE_OP_POST_LOAD_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        theEditor->PostLoadEvent(theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_SE_OP_POST_ACTION:
        if (inA == EE_SCREENS_SE_ACTION_SAVE)
            EESEPostAction(new EESEARust(kSEASave));
        else if (inA == EE_SCREENS_SE_ACTION_TEST)
            EESEPostAction(new EESEARust(kSEATest));
        else
            EESEPostAction(new EESEARust(kSEAExit));
        break;
    case EE_SCREENS_SE_OP_ASK_EXIT: {
        UWideString theText;
        gResource->GetString(IDS_SAVE_SCENARIO_BEFORE_EXITING, theText);
        gUIManager->DisplayYesNoCancel(theText, 1, NULL, EESESaveBeforeExit, EESEExitWithoutSaving);
        break;
    }
    case EE_SCREENS_SE_OP_EXIT_AFTER_SAVE:
        gUIManager->GetScenarioEditorForm()->SetExitAfterSave(true);
        break;
    }
    return 0;
}
