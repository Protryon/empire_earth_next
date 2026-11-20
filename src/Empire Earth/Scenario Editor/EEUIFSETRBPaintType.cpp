// C:\EEX\Empire Earth\Scenario Editor\EEUIFSETRBPaintType.cpp (file name inferred: no __FILE__). 0x1ac48b..0x1afa90,
// with the functions of EEUIESLLoadScreen.cpp folded in (the editor's tool bar events, EESMDialogMessage's
// Load/Save; the load screen event is a screen code, ee_screens save_load.rs). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIEvent.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EESEARust.h"
#include "lle/UResourceFile.h"
#include "ee/EEUserInterface.h"
#include "lle/GEViewport.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"

// These are genuine original RTTI classes. Their original header placement is provisional;
// first construction is in the editor toolbar builder 0x19f0b1 (CreateToolbarButtons, EEUIFScenarioEditor.cpp).
// All three are sizeof(UIEvent) = 0x0c, with no additional fields.





#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/GETerrainMesh.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEARust.h"
#include "ee/EEScenarioManager.h"
#include "lle/UIListBox.h"
#include "lle/UIRadioButton.h"
#include "lle/GEText.h"
#include "ee/EESEUnit.h"
#include "lle/UIEditBox.h"



#include "rust/game/screens.h"
#include "lle/UICheckBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
// 0x1ac48b..0x1af2a9 (the EEUIFSETRBPaintType ctor, EEUIFSETerrain's members and the unit's template COMDATs)
// moved to EEUIFSETerrain.cpp (one translation unit; its UIListBox<vector<GEColor> > /
// UIListBox<EESEUITTData> COMDATs are what that file compiles).

// The host of the editor form's events whole in Rust (EE_SCREENS_SEE_OP_*; EESEERust in ee/EESEARust.h)
void FillScenarioComboBox(UIComboBox* inComboBox, UITextLabel* inLabel, bool inAreas);   // EESETSEffect.cpp
void FillAreaObjectComboBox(UITextLabel* inLabel, UIComboBox* inComboBox, bool inAreas);   // EESETSEffect.cpp

static_assert(kSEEHideUI == EE_SCREENS_SEE_HIDE_UI && kSEEChangeViewport == EE_SCREENS_SEE_CHANGE_VIEWPORT &&
                  kSEEQuickLoad == EE_SCREENS_SEE_QUICK_LOAD && kSEEQuickSave == EE_SCREENS_SEE_QUICK_SAVE &&
                  kSEESliderChange == EE_SCREENS_SEE_SLIDER_CHANGE && kSEECheckDisplay == EE_SCREENS_SEE_CHECK_DISPLAY &&
                  kSEEUseObjectArea == EE_SCREENS_SEE_USE_OBJECT_AREA &&
                  kSEECAMGModifyObject == EE_SCREENS_SEE_CAMG_MODIFY_OBJECT &&
                  kSEEMSDCameraScroll == EE_SCREENS_SEE_MSD_CAMERA_SCROLL && kSEESAAAttribute == EE_SCREENS_SEE_SAA_ATTRIBUTE &&
                  kSEEUseAreaObject == EE_SCREENS_SEE_USE_AREA_OBJECT && kSEEFaceAreaObject == EE_SCREENS_SEE_FACE_AREA_OBJECT &&
                  kSEEMapType == EE_SCREENS_SEE_MAP_TYPE && kSEESortUnitList == EE_SCREENS_SEE_SORT_UNIT_LIST &&
                  kSEEStartEpoch == EE_SCREENS_SEE_START_EPOCH && kSEEUnitLimit == EE_SCREENS_SEE_UNIT_LIMIT &&
                  EE_SCREENS_SEE_PER_PLAYER_LABEL_ID == kSERBUnitTypesPerPlayerCountLabel,
              "ee/EESEARust.h's event kinds");

// p: three pointers, i: four integers, f: four floats (EE_SCREENS_SEE_OP_*)
extern "C" int32_t ee_screenscpp_see(uint32_t inOp, void* const* inPointers, int32_t* i, float* f)
{
    void* p = inPointers[0];
    void* q = inPointers[1];
    switch (inOp) {
    case EE_SCREENS_SEE_OP_REFERENCE:
        static_cast<EEAction*>(p)->Reference();
        break;
    case EE_SCREENS_SEE_OP_DEREFERENCE:
        static_cast<EEAction*>(p)->Dereference();
        break;
    case EE_SCREENS_SEE_OP_CHECKED:
        return static_cast<UICheckBox*>(static_cast<UIEntity*>(p))->GetIsChecked() ? 1 : 0;
    case EE_SCREENS_SEE_OP_POST_VIEWPORT: {
        EESEERust* theEvent = new EESEERust(kSEEChangeViewport);
        ee_screens_see_viewport(theEvent->mEvent, f, i[0] != 0 ? 1 : 0);
        long theResult = gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        return (int32_t)theResult;
    }
    case EE_SCREENS_SEE_OP_HAS_EDITOR:
        return gUIManager->GetScenarioEditorForm() != NULL;
    case EE_SCREENS_SEE_OP_SET_GAME_VIEW:
        gUIManager->GetScenarioEditorForm()->SetGameView(GERectangle(f[0], f[1], f[2], f[3]), i[0] != 0);
        break;
    case EE_SCREENS_SEE_OP_REDRAW_ALL:
        gUIManager->RedrawAll();
        break;
    case EE_SCREENS_SEE_OP_SHOW_LOADED:
        gUIManager->GetScenarioEditorForm()->PostShowLoadedObject(static_cast<EEAction*>(p));
        break;
    case EE_SCREENS_SEE_OP_QUICK_SAVE: {
        EEAction* theAction = new EESEARust(kSEAQuickSave, static_cast<EEAction*>(p));
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_SEE_OP_RANGE:
        return (int32_t)static_cast<UISlider*>(static_cast<UIEntity*>(p))->GetCurrentRangeValue();
    case EE_SCREENS_SEE_OP_SLIDER_TEXT: {
        // the original's sprintf of its UWideString format: the float value passed as a double through `...`
        UWideString theText;
        sprintf(theText, static_cast<const char*>(q), (double)f[0]);
        static_cast<UITextLabel*>(p)->SetText(theText);
        break;
    }
    case EE_SCREENS_SEE_OP_SET_DISPLAY:
        static_cast<UIControl*>(p)->SetDisplayMode((unsigned long)i[0]);
        break;
    case EE_SCREENS_SEE_OP_FILL_SCENARIO:
        FillScenarioComboBox(static_cast<UIComboBox*>(p), static_cast<UITextLabel*>(q), i[0] != 0);
        break;
    case EE_SCREENS_SEE_OP_SET_SELECTION:
        static_cast<UIComboBox*>(p)->SetCurrentSelection((unsigned long)i[0]);
        break;
    case EE_SCREENS_SEE_OP_ENTITY_ID:
        return (int32_t)static_cast<UIControl*>(static_cast<UIEntity*>(p))->GetId();
    case EE_SCREENS_SEE_OP_LIST_SELECTION: {
        unsigned long theSelection;
        return static_cast<UILBString*>(static_cast<UIEntity*>(p))->GetCurrentSelection(theSelection) == kErrorOK
                   ? (int32_t)theSelection
                   : -1;
    }
    case EE_SCREENS_SEE_OP_CAMG_ADD:   // the effect types (editor_effects.rs), the effect screen's controls
    case EE_SCREENS_SEE_OP_CAMG_REMOVE:
        ee_screens_seet_event(static_cast<EeScreensSeEffects*>(p), q, 0, inOp == EE_SCREENS_SEE_OP_CAMG_ADD ? 1 : 0,
                              (uint32_t)i[0]);
        break;
    case EE_SCREENS_SEE_OP_MSD_SHOW: ee_screens_seet_event(static_cast<EeScreensSeEffects*>(p), q, 1, 0, 0); break;
    case EE_SCREENS_SEE_OP_COMBO_SELECTION: {
        unsigned long theSelection;
        return static_cast<UIComboBox*>(static_cast<UIEntity*>(p))->GetCurrentSelection(theSelection) == kErrorOK
                   ? (int32_t)theSelection
                   : -1;
    }
    case EE_SCREENS_SEE_OP_SAA_SHOW:
        ee_screens_seet_event(static_cast<EeScreensSeEffects*>(p), q, 2, 0, (uint32_t)i[0]);
        break;
    case EE_SCREENS_SEE_OP_FILL_AREA_OBJECT: {
        UIForm* theForm = static_cast<UIForm*>(p);
        FillAreaObjectComboBox(static_cast<UITextLabel*>(theForm->GetControlById(i[0])),
                               static_cast<UIComboBox*>(theForm->GetControlById(i[1])), i[2] != 0);
        break;
    }
    case EE_SCREENS_SEE_OP_MAP_ELEVATION:
        ee_screens_semp_call((EeScreensSeMap*)ee_screens_segr_tab(static_cast<EeScreensSeGroup*>(p)),   // the map tab's group
                             EE_SCREENS_SEMP_SHOW_ELEVATION, i[0] != 0);
        break;
    case EE_SCREENS_SEE_OP_REFRESH_UNIT_LIST:
        static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p))->UnitCall(EE_SCREENS_SEUN_REFRESH);
        break;
    case EE_SCREENS_SEE_OP_EPOCH_COUNT:
        return (int32_t)EEEpochCount();
    case EE_SCREENS_SEE_OP_LINE_COUNT:
        return (int32_t)static_cast<UIComboBox*>(p)->GetListBox()->GetLineCount();
    case EE_SCREENS_SEE_OP_FILL_EPOCHS: {
        UWideString theText;
        std::vector<UWideString> theNames;
        for (unsigned long k = (unsigned long)i[0]; k < (unsigned long)EEEpochCount(); k++) {
            gResource->GetString(EEEpochTitleID(k), theText);
            theNames.push_back(theText);
        }
        static_cast<UIComboBox*>(p)->DeleteAllEntries();
        static_cast<UIComboBox*>(p)->AppendStringArray(theNames);
        break;
    }
    case EE_SCREENS_SEE_OP_MAX_PLAYERS:
        return (int32_t)gWorld.mMaxPlayers;
    case EE_SCREENS_SEE_OP_LABEL_NUMBER:
        static_cast<UITextLabel*>(static_cast<UIForm*>(p)->GetControlById(i[0]))->SetText((unsigned long)(uint32_t)i[1]);
        break;
    }
    return 0;
}

// (EESMDialogMessage::Load and Save: Scenario Manager/EEScenarioManager.cpp, with its constructors)
