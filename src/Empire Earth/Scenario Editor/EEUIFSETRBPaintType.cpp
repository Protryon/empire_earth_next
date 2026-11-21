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
                  EE_SCREENS_SEE_PER_PLAYER_LABEL_ID == EE_SCREENS_SE_RB_UNIT_TYPES_PER_PLAYER_COUNT_LABEL,
              "ee/EESEARust.h's event kinds");

// p: three pointers, i: four integers, f: four floats (EE_SCREENS_SEE_OP_*): the ops left of the host (the toolkit's
// in editor_direct.rs, the sequences and the Rust objects' calls in editor_tabsx.rs, over ee_screenscpp_seex)
extern "C" int32_t ee_screenscpp_see(uint32_t inOp, void* const* inPointers, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEE_OP_HAS_EDITOR:
        return gUIManager->GetScenarioEditorForm() != NULL;
    case EE_SCREENS_SEE_OP_EPOCH_COUNT:
        return (int32_t)EEEpochCount();
    case EE_SCREENS_SEE_OP_MAX_PLAYERS:
        return (int32_t)gWorld.mMaxPlayers;
    }
    return 0;
}

extern UWideString gSefxText;   // the editor's statements' text (EEUIFScenarioEditor.cpp)

namespace {

int32_t SeexOut(int32_t* i, const void* inObject)
{
    i[0] = (int32_t)(uint32_t)(uintptr_t)inObject;
    i[1] = (int32_t)(uint32_t)((uint64_t)(uintptr_t)inObject >> 32);
    return 0;
}
std::vector<UWideString> sSeexNames;   // EE_SCREENS_SEEX_NAMES_*'s epoch titles

}   // namespace

// the events' single statements (ee_screens editor_tabsx.rs EE_SCREENS_SEEX_*: p0 the object, p1 a second)
extern "C" int32_t ee_screenscpp_seex(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEEX_REFERENCE: static_cast<EEAction*>(p[0])->Reference();
        return 0;
    case EE_SCREENS_SEEX_NEW_VIEWPORT: return SeexOut(i, static_cast<UIEvent*>(new EESEERust(kSEEChangeViewport)));
    case EE_SCREENS_SEEX_EVENT_RUST: return SeexOut(i, static_cast<EESEERust*>(static_cast<UIEvent*>(p[0]))->mEvent);
    case EE_SCREENS_SEEX_POST_EVENT: return (int32_t)gUserInterface->PostEvent(static_cast<UIEvent*>(p[0]), NULL);
    case EE_SCREENS_SEEX_NEW_QUICK_SAVE: return SeexOut(i, static_cast<EEAction*>(new EESEARust(kSEAQuickSave, static_cast<EEAction*>(p[0]))));
    case EE_SCREENS_SEEX_PRINT: sprintf(gSefxText, static_cast<const char*>(p[1]), (double)f[0]);
        return 0;
    case EE_SCREENS_SEEX_LABEL_TEXT: static_cast<UITextLabel*>(p[0])->SetText(gSefxText);
        return 0;
    case EE_SCREENS_SEEX_FILL_SCENARIO: FillScenarioComboBox(static_cast<UIComboBox*>(p[0]), static_cast<UITextLabel*>(p[1]), i[0] != 0);
        return 0;
    case EE_SCREENS_SEEX_FILL_AREA_OBJECT: FillAreaObjectComboBox(static_cast<UITextLabel*>(static_cast<UIForm*>(p[0])->GetControlById(i[0])), static_cast<UIComboBox*>(static_cast<UIForm*>(p[0])->GetControlById(i[1])), i[2] != 0);
        return 0;
    case EE_SCREENS_SEEX_NAMES_CLEAR: sSeexNames.clear();
        return 0;
    case EE_SCREENS_SEEX_EPOCH_TITLE: gResource->GetString(EEEpochTitleID((unsigned long)i[0]), gSefxText);
        return 0;
    case EE_SCREENS_SEEX_NAMES_PUSH: sSeexNames.push_back(gSefxText);
        return 0;
    case EE_SCREENS_SEEX_COMBO_CLEAR: static_cast<UIComboBox*>(p[0])->DeleteAllEntries();
        return 0;
    case EE_SCREENS_SEEX_COMBO_APPEND: static_cast<UIComboBox*>(p[0])->AppendStringArray(sSeexNames);
        return 0;
    case EE_SCREENS_SEEX_EPOCH_COUNT: return (int32_t)EEEpochCount();
    }
    return 0;
}

// (EESMDialogMessage::Load and Save: Scenario Manager/EEScenarioManager.cpp, with its constructors)
