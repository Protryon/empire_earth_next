// C:\EEX\Empire Earth\Scenario Editor\EESETSObject.cpp (file name inferred: no __FILE__; RTTI class EESETSObject).
// The trigger screen's object sub-screen: 0x18efb7..0x19393b (the ctor first, as EESETSArea/EESETSTrigger; then the
// out-of-line inlines it refused, the small control classes EESETSOUnitList/EESETSAMouseClick/EESETSALasso/
// EESETSAComboBox/EESETSAEditBox, the ??_G block, the dtor and the slots, the unit type sort's COMDATs and the event
// ??_Gs). After EESETSETSetObjectStance.cpp, before EESETSTrigger.cpp (0x19393b). Unit flags: the exe module set.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
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

#include "lle/UITriStateCheckBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/USingleton.h"
#include "ee/EEMedia.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIShell.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"

// Unit action names are the real EEUnitAction::sActionNames table at 0x563668.

// 0x563678: the unit action ids by name (the object screen's action combo box) are EEUnitAction::sActionIDs
#include "ee/EEStringIDs.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"
// The unit type combo box's sort (0x19330c, the std::sort instantiation 0x193077..0x19370e): type 0 first, then the
// unit classes by name (provisional name; the comparison is ee_screens editor_trigx.rs unit_type_less)
struct EESETSOUnitTypeNameLess {
    bool operator()(const unsigned long& inA, const unsigned long& inB) const { return ee_screens_trgx_unit_type_less(inA, inB); }
};

// The object screen whole in Rust (ee_screens editor_object.rs): the constructor (0x58efb7), the destructor
// (0x591973), Sort, MoveDown, MoveUp, Delete, ClearUnitList, Deactivate, Refresh, Duplicate, Activate (0x591d63),
// Store (0x5920f1), GetNames, GetIsSelectOnMap, Hide, Load (0x5925a2), New, UpdateSelection (0x592ad3),
// CheckChanged, Show and MouseMove are its members; the class forwards. The control classes below stay C++, and
// this unit is the screen's host (ee_screenscpp_seob).

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// ---- the host (editor_object.rs: the ops from 100; the action names and the toolkit's texts are answered in Rust,
// editor_direct.rs)

static_assert(EE_SCREENS_SEOB_IDS_ANY_CLASS == IDS_ANY_CLASS && EE_SCREENS_SEOB_IDS_NONE == IDS_NONE &&
                  EE_SCREENS_SEOB_IDS_SE_UNIT_GROUP_UNITS == IDS_SE_UNIT_GROUP_UNITS &&
                  EE_SCREENS_SEOB_IDS_FAMILY == IDS_FAMILY && EE_SCREENS_SEOB_IDS_SELECT_ON_MAP == IDS_SELECT_ON_MAP &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 1 == IDS_OBJECT_SPECIFICATION &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 2 == IDS_TRIGGER_EDITOR_OWNER &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 3 == IDS_CLASS &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 5 == IDS_TRIGGER_EDITOR_MIN &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 6 == IDS_TRIGGER_EDITOR_MAX &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 7 == IDS_DYNAMIC && EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 8 == IDS_IN_AREA &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 9 == IDS_VISIBLE_BY &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 10 == IDS_HAS_STATE &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 11 == IDS_SELECTED_BY &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 12 == IDS_HAS_ATTRIBUTE &&
                  EE_SCREENS_SEOB_IDS_UNIT_SELECTED_LIST == IDS_UNIT_SELECTED_LIST &&
                  EE_SCREENS_SEOB_IDS_TRIGGER_EDITOR_USE_OBJECT == IDS_TRIGGER_EDITOR_USE_OBJECT &&
                  EE_SCREENS_SEOB_IDS_TRIGGER_PROXIMITY_FIRST == IDS_TRIGGER_PROXIMITY_FIRST &&
                  EE_SCREENS_SEOB_IDS_TRIGGER_ATTRIBUTE_FIRST == IDS_TRIGGER_ATTRIBUTE_FIRST,
              "the strings");
static_assert(EE_SCREENS_SEOB_AREA_CHECK == EE_SCREENS_SE_TS_OBJECT_AREA_CHECK && EE_SCREENS_SEOB_NEAR_CHECK == EE_SCREENS_SE_TS_OBJECT_NEAR_CHECK &&
                  EE_SCREENS_SEOB_VISIBLE_BY_CHECK == EE_SCREENS_SE_TS_OBJECT_VISIBLE_BY_CHECK &&
                  EE_SCREENS_SEOB_HAS_STATE_CHECK == EE_SCREENS_SE_TS_OBJECT_HAS_STATE_CHECK &&
                  EE_SCREENS_SEOB_SELECTED_BY_CHECK == EE_SCREENS_SE_TS_OBJECT_SELECTED_BY_CHECK &&
                  EE_SCREENS_SEOB_SELECT_UNITS == EE_SCREENS_SE_TS_OBJECT_SELECT_UNITS,
              "the ids");

namespace {

EEUIFScenarioEditor* SeobEditor(void* inControl)
{
    return static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(inControl)->GetForm());
}

// a scenario object's number field (EE_SCREENS_SEOB_F_*; the three flags and the parent aside)
unsigned long* SeobField(EESMObject* o, int32_t inField)
{
    switch (inField) {
    case EE_SCREENS_SEOB_F_OWNER: return &o->mOwner;
    case EE_SCREENS_SEOB_F_FAMILY: return &o->mFamily;
    case EE_SCREENS_SEOB_F_CLASS: return &o->mClass;
    case EE_SCREENS_SEOB_F_MIN: return &o->mMinCount;
    case EE_SCREENS_SEOB_F_MAX: return &o->mMaxCount;
    case EE_SCREENS_SEOB_F_AREA: return &o->mArea;
    case EE_SCREENS_SEOB_F_NEAR_MODE: return &o->mNearMode;
    case EE_SCREENS_SEOB_F_NEAR_OBJECT: return &o->mNearObject;
    case EE_SCREENS_SEOB_F_PARENT: return &o->mParent;
    case EE_SCREENS_SEOB_F_ATTRIBUTE: return &o->mAttribute;
    case EE_SCREENS_SEOB_F_ATTRIBUTE_MIN: return &o->mAttributeMin;
    case EE_SCREENS_SEOB_F_ATTRIBUTE_MAX: return &o->mAttributeMax;
    case EE_SCREENS_SEOB_F_VISIBLE_BY: return &o->mVisibleByPlayer;
    case EE_SCREENS_SEOB_F_STATE: return &o->mState;
    case EE_SCREENS_SEOB_F_SELECTED_BY: return &o->mSelectedByPlayer;
    case EE_SCREENS_SEOB_F_IN_AREA_CHECK: return &o->mInAreaCheck;
    case EE_SCREENS_SEOB_F_NEAR_CHECK: return &o->mNearCheck;
    case EE_SCREENS_SEOB_F_HAS_ATTRIBUTE_CHECK: return &o->mHasAttributeCheck;
    case EE_SCREENS_SEOB_F_VISIBLE_BY_CHECK: return &o->mVisibleByCheck;
    case EE_SCREENS_SEOB_F_HAS_STATE_CHECK: return &o->mHasStateCheck;
    case EE_SCREENS_SEOB_F_SELECTED_BY_CHECK: return &o->mSelectedByCheck;
    }
    return NULL;
}

// a scenario object's flag (editor_trigx.rs seob_flag: which fields are flags)
bool* SeobFlag(EESMObject* o, int32_t inField)
{
    switch (inField) {
    case EE_SCREENS_SEOB_F_SELECT_ON_MAP: return &o->mIsSelectOnMap;
    case EE_SCREENS_SEOB_F_DYNAMIC: return &o->mIsDynamic;
    case EE_SCREENS_SEOB_F_PERCENT: return &o->mIsAttributePercent;
    }
    return NULL;
}

UWideString SeobBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}

std::vector<unsigned long> sSeobTypes;   // EE_SCREENS_SEOB_OP_UNIT_TYPES' kept unit types
std::vector<unsigned long> sSeobUnits;   // EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS' kept units

}   // namespace

// the screen's own ops (editor_trigx.rs composes the sequences and the ops on a scenario object)
extern "C" int32_t ee_screenscpp_seob(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    (void)f;
    switch (inOp) {
    case EE_SCREENS_SEOB_OP_UNIT_SET_COUNT: return (int32_t)gUnitSetManager.GetSize();
    case EE_SCREENS_SEOB_OP_UNIT_SET_SHOWN:
        return gUnitSetManager.GetSet(i[0]) != NULL && gUnitSetManager.GetSet(i[0])->ShowInEditor() == true;
    case EE_SCREENS_SEOB_OP_CLEAR_SELECTION: SeobEditor(p[0])->ClearSelection(); return 0;
    case EE_SCREENS_SEOB_OP_PLAYER_COUNT: return (int32_t)SeobEditor(p[0])->PlayerCall(EE_SCREENS_SEPL_PLAYER_COUNT);
    case EE_SCREENS_SEOB_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_OBJECTS,
                                          *static_cast<std::vector<UWideString>*>(p[0]));
        return 0;
    case EE_SCREENS_SEOB_OP_SELECTED_UNIT_ID:
        return (int32_t) static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetSelectedUnit(i[0])->mUnitID;
    case EE_SCREENS_SEOB_OP_SELECTED_COUNT:
        return (int32_t) static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetSelectedUnitCount();
    case EE_SCREENS_SEOB_OP_START_SELECTION:
        static_cast<EESEUICTriggerSelectUnits*>(p[0])->StartSelection(GE2DPoint(f[0], f[1]));
        return 0;
    }
    return 0;
}

// the screen's single statements (editor_trigx.rs EE_SCREENS_SEOBX_*, 200..: p0 and p1 the objects, p2 the out
// word of a made or found one, i and f the numbers, a text p1's i0 bytes)
extern "C" int32_t ee_screenscpp_seobx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[2]);
    EESMObject* o = static_cast<EESMObject*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEOBX_NEW_CLICK:
        *theOut = new EESETSAMouseClick;
        return 0;
    case EE_SCREENS_SEOBX_NEW_LASSO:
        *theOut = new EESETSALasso;
        return 0;
    case EE_SCREENS_SEOBX_NEW_SELECT:
        *theOut = new EESEUICTriggerSelectUnits(static_cast<UIForm*>(p[0]), EE_SCREENS_SE_TS_OBJECT_SELECT_UNITS, (unsigned long)i[0],
                                                static_cast<EESETSAMouseClick*>(p[1]), static_cast<EESETSALasso*>(*theOut));
        return 0;
    case EE_SCREENS_SEOBX_ADD_CONTROL:
        static_cast<UIForm*>(p[0])->AddControl(static_cast<EESEUICTriggerSelectUnits*>(p[1]), 0);
        return 0;
    case EE_SCREENS_SEOBX_LASSO_RELEASE:
        static_cast<EESETSALasso*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEOBX_CLICK_RELEASE:
        static_cast<EESETSAMouseClick*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEOBX_TYPES_CLEAR:
        sSeobTypes.clear();
        return 0;
    case EE_SCREENS_SEOBX_TYPES_PUSH:
        sSeobTypes.push_back((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEOBX_TYPES_SORT:
        std::sort(sSeobTypes.begin(), sSeobTypes.end(), EESETSOUnitTypeNameLess());
        return 0;
    case EE_SCREENS_SEOBX_TYPES_COUNT: return (int32_t)sSeobTypes.size();
    case EE_SCREENS_SEOBX_TYPES_AT: return (int32_t)sSeobTypes[(size_t)i[0]];
    case EE_SCREENS_SEOBX_FLAG: return *SeobFlag(o, i[0]);
    case EE_SCREENS_SEOBX_FIELD: return (int32_t)*SeobField(o, i[0]);
    case EE_SCREENS_SEOBX_SET_FLAG:
        *SeobFlag(o, i[0]) = i[1] != 0;
        return 0;
    case EE_SCREENS_SEOBX_SET_PARENT:
        o->SetParent((long)i[1]);
        return 0;
    case EE_SCREENS_SEOBX_SET_FIELD:   // (a none selection is all ones, as the original's unsigned long)
        *SeobField(o, i[0]) = (unsigned long)(long)i[1];
        return 0;
    case EE_SCREENS_SEOBX_SET_NAME:
        o->SetName(SeobBytes(p[1], i[0]));
        return 0;
    case EE_SCREENS_SEOBX_SET_UNITS:
        o->SetUnits(std::vector<unsigned long>(static_cast<const uint32_t*>(p[1]), static_cast<const uint32_t*>(p[1]) + i[0]));
        return 0;
    case EE_SCREENS_SEOBX_UNITS_GET:
        o->GetUnits(sSeobUnits);
        return 0;
    case EE_SCREENS_SEOBX_UNITS_COUNT: return (int32_t)sSeobUnits.size();
    case EE_SCREENS_SEOBX_UNITS_AT: return (int32_t)sSeobUnits[(size_t)i[0]];
    case EE_SCREENS_SEOBX_WORLD_UNIT:
        *theOut = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)(uint32_t)i[0]));
        return 0;
    case EE_SCREENS_SEOBX_SELECT_UNIT:
        SeobEditor(p[0])->SelectUnit(static_cast<EEUnit*>(p[1]));
        return 0;
    case EE_SCREENS_SEOBX_SELECTED_COUNT:
        return (int32_t) static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetSelectedUnitCount();
    case EE_SCREENS_SEOBX_SELECTED_NAME_ID:
        return (int32_t) static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetSelectedUnit(i[0])->mUnitClass->mNameStringID;
    case EE_SCREENS_SEOBX_IN_VIEW: return gViewport.GetViewRectangle().Contains(GE2DPoint(f[0], f[1]));
    case EE_SCREENS_SEOBX_LBUTTON_JUST: return UIManager::GetLMouseButton()->JustPressed();
    case EE_SCREENS_SEOBX_RBUTTON_JUST: return UIManager::GetRMouseButton()->JustPressed();
    }
    return 0;
}
