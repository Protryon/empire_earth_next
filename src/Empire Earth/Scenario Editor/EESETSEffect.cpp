// C:\EEX\Empire Earth\Scenario Editor\EESETSEffect.cpp (file name inferred: no __FILE__; RTTI class EESETSEffect). The
// trigger screen's effect sub-screen: 0x182629..0x182dda (ctor, ??_G, dtor, ChangeEffectAction/Type, the slots in the
// order of their bodies, the GetEffect COMDAT and EESEEChangeEffectType's ??_G), the twin of EESETSCondition.cpp.
// After EESETSEComboBox.cpp, before EESETSETAI.cpp. Unit flags: the exe module set.
// Its seven effect types (EESETSEffectType and every EESETSET* class, once EESETSETAI.cpp .. EESETSETSetObjectStance.cpp,
// 0x182dda..0x18efb7) are whole in Rust (ee_screens editor_effects.rs), as the condition screen's seven condition
// types (EESETSConditionType and its EESETSCT* classes, once EESETSConditionType.cpp; editor_conditions.rs): this
// unit keeps their two hosts, ee_screenscpp_seet and ee_screenscpp_sect, one template over the effect or condition
// screen's controls by array and index.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"


#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESMEffect.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIShell.h"
#include "ee/EESMCondition.h"
#include "ee/EESEARust.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EECPBehavior.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UIESliderChange.h"
#include "ee/EEScreenEvent.h"
// The screen is whole in Rust (ee_screens editor_subscreens.rs) with its seven effect types (editor_effects.rs): the
// constructor (0x582629), the destructor (0x5829a4), ChangeEffectAction (0x582a3b), ChangeEffectType (0x582a69),
// Sort, MoveDown, MoveUp, Delete, Duplicate, Activate (0x582b22), Store (0x582b96), GetNames, Load (0x582c55), New,
// RefreshControls, Show and Hide are its slots; the class forwards.


// ------------------------------------------------------------------------------------------------ the types' host

// the place combo boxes (once EESEEvents.cpp's, EESETSETPlayer.cpp's and EESETSETGame.cpp's; the sequences are
// ee_screens editor_trigx.rs')
void FillScenarioComboBox(UIComboBox* inComboBox, UITextLabel* inLabel, bool inAreas)
{
    ee_screens_trgx_fill_scenario(inComboBox, inLabel, inAreas);
}

void FillAreaObjectComboBox(UITextLabel* inLabel, UIComboBox* inComboBox, bool inAreas)
{
    ee_screens_trgx_fill_area_object(inLabel, inComboBox, inAreas);
}

void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox)
{
    ee_screens_trgx_fill_place(inAreas, inLabel, inComboBox);
}

namespace {


// the technologies by their editor names (editor_trigx.rs tech_less: as EESETSConditionType.cpp's
// EESETSCTResearchNameLess; once EESETSETPlayer.cpp)
struct SeetTechNameLess {
    bool operator()(const long& inLeft, const long& inRight) const { return ee_screens_trgx_tech_less(inLeft, inRight); }
};

}   // namespace

// TEST ONLY: the alignment each label was last given through the host (the editor gate's `align`; the toolkit's label
// keeps it without a getter)
std::map<const void*, unsigned long> gSeetAlignments;

namespace {

std::vector<UWideString> sSeetNames[2];   // the kept names: EE_SCREENS_SEET_OP_NAMES' (0) and the sequences' (1)
std::vector<long> sSeetTechs;             // EE_SCREENS_SEET_OP_TECHS' kept technologies
UWideString sSeetText;                    // the kept formatted text (EE_SCREENS_SEETX_FORMAT_*)


UWideString SeetBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}

GEFont* SeetFont(int32_t inFont)
{
    return inFont == EE_SCREENS_SEPL_FONT_538 ? gU531938 : gU531934;
}

UIControl* SeetControl(const int32_t* i)
{
    return reinterpret_cast<UIControl*>((uintptr_t)((uint64_t)(uint32_t)i[0] | ((uint64_t)(uint32_t)i[1] << 32)));
}

// the controls by array (EE_SCREENS_SEET_A_*): the effect screen's and the condition screen's records are Rust's
// (ee_screens editor_direct.rs: made, freed, their slots and the toolkit's ops); the cases below read a member
// through it

struct SeetC {
    void* mControls;
    bool mCondition;
    template <class T = UIControl>
    T* At(int32_t a, int32_t k) const
    {
        return static_cast<T*>(static_cast<UIControl*>(ee_screens_seet_slot(mControls, mCondition, (uint32_t)a, (uint32_t)k)));
    }
    UITextLabel* Label(int32_t k) const { return At<UITextLabel>(EE_SCREENS_SEET_A_LABEL, k); }
    UILBString* List(int32_t k) const { return At<UILBString>(EE_SCREENS_SEET_A_LIST, k); }
    UISlider* Slider(int32_t k) const { return At<UISlider>(EE_SCREENS_SEET_A_SLIDER, k); }
};

// The ops both hosts share that the toolkit's Rust does not answer and that are not composed in Rust
// (editor_trigx.rs: p0 the controls, p1 the entry or a handle, p2 a buffer or an out word; i0 the control array, i1
// its index, i2 and i3 the arguments; f the floats)
int32_t SeetOp(uint32_t inOp, const SeetC& c, void* const* p, int32_t* i, float* f)
{
    int32_t a = i[0];
    int32_t k = i[1];
    switch (inOp) {
    case EE_SCREENS_SEET_OP_SET_RECT:
        c.At(a, k)->SetRectangle(GERectangle(GE2DPoint(f[0], f[1]), GE2DPoint(f[2], f[3])));
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_PUSH:
        EESEEPushControl(static_cast<EESEERust*>(static_cast<UIEvent*>(p[1])), c.At(a, k));
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_PROCESS:
        static_cast<UIEvent*>(p[1])->ProcessEvent(gUIManager->GetScenarioEditorForm(), c.At(a, k));
        return 0;
    case EE_SCREENS_SEET_OP_CHECK: c.At<UIRadioButton>(EE_SCREENS_SEET_A_RADIO, k)->Check(); return 0;
    case EE_SCREENS_SEET_OP_PLAYER:
        return (int32_t)EEUIShell::Op(c.At(EE_SCREENS_SEET_A_PLAYER, k), EE_SCREENS_SKOP_SELECTED_PLAYER);
    case EE_SCREENS_SEET_OP_SET_PLAYER:
        EEUIShell::Op(c.At(EE_SCREENS_SEET_A_PLAYER, k), EE_SCREENS_SKOP_SET_SELECTED_PLAYER, (uint32_t)i[2]);
        return 0;
    case EE_SCREENS_SEET_OP_PLAYER_FLAGS:
        EEUIShell::Op(c.At(EE_SCREENS_SEET_A_PLAYER, k), EE_SCREENS_SKOP_SET_FLAGS, i[2]);
        return 0;
    case EE_SCREENS_SEET_OP_SCEN_MODE:
        EEUIShell::Op(c.At(EE_SCREENS_SEET_A_SCEN, k), EE_SCREENS_SKOP_SET_MODE, i[2]);
        return 0;
    case EE_SCREENS_SEET_OP_LIST_INSERT:
        c.List(k)->InsertLine(SeetBytes(p[2], i[2]), (unsigned long)i[3]);
        return 0;
    case EE_SCREENS_SEET_OP_LIST_DELETE: c.List(k)->DeleteLine((unsigned long)i[2]); return 0;
    case EE_SCREENS_SEET_OP_FORM_DISPLAY:
        c.At(EE_SCREENS_SEET_A_TEXT, 0)->GetForm()->GetControlById((unsigned long)i[2])->SetDisplayMode((unsigned long)i[3]);
        return 0;
    case EE_SCREENS_SEET_OP_SLIDER_MODIFIER:
        static_cast<UIESliderChange*>(c.Slider(k)->GetTriggerEvent(kControlTriggerRangeChanged))->SetModifier(f[0]);
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_RELEASE: static_cast<UIEvent*>(p[1])->Dereference(); return 0;
    case EE_SCREENS_SEET_OP_SET_NAME_BOX:
        gUIManager->GetScenarioEditorForm()->GetTriggerBox(false)->SetText(SeetBytes(p[2], i[2]));
        return 0;
    case EE_SCREENS_SEET_OP_NAME_BOX:
        return EESEWrite(p[2], k, gUIManager->GetScenarioEditorForm()->GetTriggerBox(false)->GetText(0));
    case EE_SCREENS_SEET_OP_NAME: return EESEWrite(p[2], k, sSeetNames[0][(size_t)i[2]]);
    case EE_SCREENS_SEET_OP_CALAMITY_COUNT: return (int32_t)EECalamityManager::GetInstance()->GetCalamityClassCount();
    case EE_SCREENS_SEET_OP_GRAPHIC_COUNT: return (int32_t)EEGraphicEffect::GetCount();
    case EE_SCREENS_SEET_OP_BEHAVIOR_COUNT: return (int32_t)(sizeof(gCPBehaviors) / sizeof(EECPBehavior));
    case EE_SCREENS_SEET_OP_UNIT_TYPE_COUNT: return (int32_t)EEUnitClass::GetUnitTypes().size();
    case EE_SCREENS_SEET_OP_UNIT_NAME: return (int32_t)gUnitClassTable[i[2]]->mNameStringID;
    case EE_SCREENS_SEET_OP_TECH: return (int32_t)sSeetTechs[(size_t)i[2]];
    case EE_SCREENS_SEET_OP_SET_TEXT_BOX:
        gUIManager->GetScenarioEditorForm()->GetTriggerBox(true)->SetText(SeetBytes(p[2], i[2]));
        return 0;
    case EE_SCREENS_SEET_OP_PLAYER_COUNT:
        return (int32_t)static_cast<EEUIFScenarioEditor*>(c.At(EE_SCREENS_SEET_A_TEXT, 0)->GetForm())->PlayerCall(EE_SCREENS_SEPL_PLAYER_COUNT);
    case EE_SCREENS_SEET_OP_SET_PLAYER_COUNT:
        EEUIShell::Op(c.At(EE_SCREENS_SEET_A_PLAYER, k), EE_SCREENS_SKOP_SET_PLAYER_COUNT, (uint32_t)i[2]);
        return 0;
    }
    return 0;
}

}   // namespace

// The effect screen's and types' host (editor_subscreens.rs, editor_effects.rs): the shared ops over the effect
// screen's controls (editor_direct.rs answers the toolkit's first, editor_trigx.rs composes the sequences); the
// effect's objects, the effects' names
extern "C" int32_t ee_screenscpp_seet(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESMEffect* theEffect = static_cast<EESMEffect*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEET_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_EFFECTS, *static_cast<std::vector<UWideString>*>(p[1]));
        return 0;
    case EE_SCREENS_SEET_OP_OBJECT_COUNT: return (int32_t)theEffect->mObjects.size();
    case EE_SCREENS_SEET_OP_OBJECT: return (int32_t)theEffect->mObjects[(size_t)i[2]];
    }
    return SeetOp(inOp, SeetC{p[0], false}, p, i, f);
}

// The condition screen's and types' host (editor_subscreens.rs, editor_conditions.rs): the shared ops over the
// condition screen's controls (as the effect screen's); the conditions' names
extern "C" int32_t ee_screenscpp_sect(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEET_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_CONDITIONS, *static_cast<std::vector<UWideString>*>(p[1]));
        return 0;
    }
    return SeetOp(inOp, SeetC{p[0], true}, p, i, f);
}

// The trigger tab family's single statements (editor_trigx.rs EE_SCREENS_SEETX_*, 0..199: the Rust composes the
// hosts' sequences; p0 and p1 the objects, i and f the numbers, p2 the out word of an object or the buffer of a
// text (capacity i3), a text passed in p1's i0 bytes, or p0's for a statement on no object)
extern "C" int32_t ee_screenscpp_seetx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[2]);
    switch (inOp) {
    case EE_SCREENS_SEETX_ALIGN:
        static_cast<UITextLabel*>(p[0])->SetAlignment((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEETX_ALIGN_NOTE:
        gSeetAlignments[p[0]] = (unsigned long)i[0];
        return 0;
    case EE_SCREENS_SEETX_RADIO_TEXT:
        static_cast<UIRadioButton*>(p[0])->SetText(SeetBytes(p[1], i[0]), SeetFont(i[1]));
        return 0;
    case EE_SCREENS_SEETX_CHECK_TEXT:
        static_cast<UICheckBox*>(p[0])->SetText(SeetBytes(p[1], i[0]), SeetFont(i[1]));
        return 0;
    case EE_SCREENS_SEETX_CHECK_GETEXT:
        *theOut = static_cast<UICheckBox*>(p[0])->GetTextLabel()->GetGEText();
        return 0;
    case EE_SCREENS_SEETX_RADIO_GETEXT:
        *theOut = static_cast<UIRadioButton*>(p[0])->GetTextLabel()->GetGEText();
        return 0;
    case EE_SCREENS_SEETX_LIST_GETEXT:
        *theOut = static_cast<UILBString*>(p[0])->GetGEText();
        return 0;
    case EE_SCREENS_SEETX_YELLOW:
        static_cast<GEText*>(p[0])->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        return 0;
    case EE_SCREENS_SEETX_TRANSPARENT:
        static_cast<GEText*>(p[0])->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SEETX_EDIT_FONT_530:
        static_cast<UIEditBox*>(p[0])->GetGEText()->SetFont(gU531930);
        return 0;
    case EE_SCREENS_SEETX_COMBO_FONT_530:
        static_cast<UIComboBox*>(p[0])->GetGEText()->SetFont(gU531930);
        return 0;
    case EE_SCREENS_SEETX_COMBO_LIST_FONT_530:
        static_cast<UIComboBox*>(p[0])->GetListBox()->GetGEText()->SetFont(gU531930);
        return 0;
    case EE_SCREENS_SEETX_NAMES_CLEAR:
        sSeetNames[i[0]].clear();
        return 0;
    case EE_SCREENS_SEETX_NAMES_GET:
        gWorld.mScenarioManager->GetNames((unsigned long)i[1], sSeetNames[i[0]]);
        return 0;
    case EE_SCREENS_SEETX_NAMES_COUNT: return (int32_t)sSeetNames[i[0]].size();
    case EE_SCREENS_SEETX_NAMES_AT: return EESEWrite(p[2], i[3], sSeetNames[i[0]][(size_t)i[1]]);
    case EE_SCREENS_SEETX_COMBO_APPEND_NAMES:
        static_cast<UIComboBox*>(p[0])->AppendStringArray(sSeetNames[i[0]]);
        return 0;
    case EE_SCREENS_SEETX_LIST_BG_OPACITY:
        static_cast<UILBString*>(p[0])->SetBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEETX_LIST_SCROLL_OPACITY:
        static_cast<UILBString*>(p[0])->SetScrollBarBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEETX_EV_SCREEN:
        *theOut = EECreateScreenEvent((uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_EV_SEE:
        *theOut = static_cast<UIEvent*>(new EESEERust((uint32_t)i[0]));
        return 0;
    case EE_SCREENS_SEETX_EV_SLIDER:
        *theOut = static_cast<UIEvent*>(new UIESliderChange(static_cast<UITextLabel*>(p[0]), f[0], (long)i[0]));
        return 0;
    case EE_SCREENS_SEETX_EV_SLIDER_FORMAT:
        *theOut = static_cast<UIEvent*>(EESEENewSliderChange(static_cast<UITextLabel*>(p[0]), static_cast<const char*>(p[1]), f[0], f[1]));
        return 0;
    case EE_SCREENS_SEETX_EV_USE_OBJECT_AREA:
        *theOut = static_cast<UIEvent*>(
            EESEENewUseObjectArea(static_cast<UITextLabel*>(p[0]), static_cast<UIComboBox*>(p[1]), SeetControl(i)));
        return 0;
    case EE_SCREENS_SEETX_EV_OBJECTS:
        *theOut = static_cast<UIEvent*>(EESEENewObjects((uint32_t)i[0], p[1], p[0]));
        return 0;
    case EE_SCREENS_SEETX_EVENT_ADD_STRING:
        static_cast<UIESliderChange*>(static_cast<UIEvent*>(p[1]))->AddString(f[0], SeetBytes(p[0], i[0]));
        return 0;
    case EE_SCREENS_SEETX_FORMAT_NUMBER:
        sprintf(sSeetText, SeetBytes(p[1], i[0]), (unsigned long)(uint32_t)i[1]);
        return 0;
    case EE_SCREENS_SEETX_FORMAT_FLOAT:
        sprintf(sSeetText, static_cast<const char*>(p[0]), (double)f[0]);
        return 0;
    case EE_SCREENS_SEETX_KEPT_TEXT: return EESEWrite(p[2], i[3], sSeetText);
    case EE_SCREENS_SEETX_OBJECT:
        *theOut = gWorld.mScenarioManager->GetScenarioObject((unsigned long)(uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_OBJECT_NAME: return EESEWrite(p[2], i[3], static_cast<EESMObject*>(p[0])->mName);
    case EE_SCREENS_SEETX_TRIGGER:
        *theOut = gWorld.mScenarioManager->GetTrigger((unsigned long)(uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_TRIGGER_NAME: return EESEWrite(p[2], i[3], static_cast<EESMTrigger*>(p[0])->mName);
    case EE_SCREENS_SEETX_CALAMITY:
        *theOut = EECalamityManager::GetInstance()->GetCalamityClass((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEETX_CALAMITY_ID: return (int32_t)static_cast<EECalamityClass*>(p[0])->mID;
    case EE_SCREENS_SEETX_CALAMITY_NAME: return (int32_t)static_cast<EECalamityClass*>(p[0])->mNameID;
    case EE_SCREENS_SEETX_GRAPHIC:
        *theOut = EEGraphicEffect::Get((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEETX_GRAPHIC_LISTED: return static_cast<EEGraphicEffect*>(p[0])->mIsScenarioListed == true;
    case EE_SCREENS_SEETX_GRAPHIC_ID: return (int32_t)static_cast<EEGraphicEffect*>(p[0])->mID;
    case EE_SCREENS_SEETX_GRAPHIC_NAME: return (int32_t)static_cast<EEGraphicEffect*>(p[0])->mNameStringId;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_SETTABLE: return gCPBehaviors[i[0]].mIsSettable == true ? 1 : 0;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_INT1_MIN: return gCPBehaviors[i[0]].mIntValue1Min;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_INT1_MAX: return gCPBehaviors[i[0]].mIntValue1Max;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_INT2_MIN: return gCPBehaviors[i[0]].mIntValue2Min;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_INT2_MAX: return gCPBehaviors[i[0]].mIntValue2Max;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_FLOAT_MIN: return gCPBehaviors[i[0]].mFloatSliderMin;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_FLOAT_MAX: return gCPBehaviors[i[0]].mFloatSliderMax;
    case EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_FLOAT_SCALE: return gCPBehaviors[i[0]].mFloatSliderScale;
    case EE_SCREENS_SEETX_UNIT_TYPE_CLASS:
        *theOut = gUnitClassTable[EEUnitClass::GetUnitTypes()[(size_t)i[0]]];
        return 0;
    case EE_SCREENS_SEETX_CLASS_SELECTABLE: return static_cast<EEUnitClass*>(p[0])->mIsScenarioSelectable == true;
    case EE_SCREENS_SEETX_CLASS_TYPE: return (int32_t)static_cast<EEUnitClass*>(p[0])->mUnitType;
    case EE_SCREENS_SEETX_CLASS_DOMAINS: return (int32_t)static_cast<EEUnitClass*>(p[0])->mDomains;
    case EE_SCREENS_SEETX_TECHS_CLEAR:
        sSeetTechs.clear();
        return 0;
    case EE_SCREENS_SEETX_TECHS_NODE_IDS:
        gTechTree.GetNodeIDs(sSeetTechs);
        return 0;
    case EE_SCREENS_SEETX_EPOCH_COUNT: return (int32_t)EEEpochCount();
    case EE_SCREENS_SEETX_TECHS_PUSH:
        sSeetTechs.push_back((long)(unsigned long)(uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_TECHS_COUNT: return (int32_t)sSeetTechs.size();
    case EE_SCREENS_SEETX_TECHS_AT: return (int32_t)sSeetTechs[(size_t)i[0]];
    case EE_SCREENS_SEETX_TECH_SHOWN: return gTechTree.Get(i[0])->mData->mShowInEditor == true;
    case EE_SCREENS_SEETX_TECH_NO_UNIT: return gTechTree.Get(i[0])->mData->mMakesUnit == false;
    case EE_SCREENS_SEETX_TECHS_ERASE:
        sSeetTechs.erase(sSeetTechs.begin() + i[0]);
        return 0;
    case EE_SCREENS_SEETX_TECHS_SORT:
        std::sort(sSeetTechs.begin(), sSeetTechs.end(), SeetTechNameLess());
        return 0;
    case EE_SCREENS_SEETX_TECH_NODE:
        *theOut = gTechTree.Get(i[0]);
        return 0;
    case EE_SCREENS_SEETX_NODE_UNIT_CLASS: return (int32_t)static_cast<EETechTreeNode*>(p[0])->mData->mUnitClass;
    case EE_SCREENS_SEETX_NODE_NAME_ID: return (int32_t)static_cast<EETechTreeNode*>(p[0])->mData->mNameID;
    case EE_SCREENS_SEETX_NODE_ID: return (int32_t)static_cast<EETechTreeNode*>(p[0])->mData->mID;
    case EE_SCREENS_SEETX_CLASS_EXISTS: return gUnitClassTable[i[0]] != NULL;
    case EE_SCREENS_SEETX_CLASS_NAME_ID: return (int32_t)gUnitClassTable[i[0]]->mNameStringID;
    case EE_SCREENS_SEETX_TRIGGER_CALL:
        gUIManager->GetScenarioEditorForm()->TriggerCall((uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_TRIGGER_BOX:
        *theOut = gUIManager->GetScenarioEditorForm()->GetTriggerBox(i[0] != 0);
        return 0;
    case EE_SCREENS_SEETX_EFFECT_ENTRY:
        *theOut = gWorld.mScenarioManager->GetEffect((unsigned long)(uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_EFFECT_FIELDS:
        *theOut = static_cast<EeScenarioEffectFields*>(static_cast<EESMEffect*>(p[0]));
        return 0;
    case EE_SCREENS_SEETX_CONDITION_ENTRY:
        *theOut = gWorld.mScenarioManager->GetCondition((unsigned long)(uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEETX_CONDITION_FIELDS:
        *theOut = static_cast<EeScenarioConditionFields*>(static_cast<EESMCondition*>(p[0]));
        return 0;
    case EE_SCREENS_SEETX_EFFECT_TEXT + 0: return EESEWrite(p[2], i[3], static_cast<EESMEffect*>(p[0])->mNewName);
    case EE_SCREENS_SEETX_EFFECT_TEXT + 1: return EESEWrite(p[2], i[3], static_cast<EESMEffect*>(p[0])->mMessage);
    case EE_SCREENS_SEETX_EFFECT_TEXT + 2: return EESEWrite(p[2], i[3], static_cast<EESMEffect*>(p[0])->mFileName);
    case EE_SCREENS_SEETX_EFFECT_TEXT + 3: return EESEWrite(p[2], i[3], static_cast<EESMEffect*>(p[0])->mDescription);
    case EE_SCREENS_SEETX_EFFECT_TEXT + 4: return EESEWrite(p[2], i[3], static_cast<EESMEffect*>(p[0])->mText);
    case EE_SCREENS_SEETX_EFFECT_TEXT + 5: return EESEWrite(p[2], i[3], static_cast<EESMEffect*>(p[0])->mName);
    case EE_SCREENS_SEETX_SET_EFFECT_TEXT + 0:
        static_cast<EESMEffect*>(p[0])->mNewName = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_EFFECT_TEXT + 1:
        static_cast<EESMEffect*>(p[0])->mMessage = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_EFFECT_TEXT + 2:
        static_cast<EESMEffect*>(p[0])->mFileName = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_EFFECT_TEXT + 3:
        static_cast<EESMEffect*>(p[0])->mDescription = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_EFFECT_TEXT + 4:
        static_cast<EESMEffect*>(p[0])->mText = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_EFFECT_TEXT + 5:
        static_cast<EESMEffect*>(p[0])->mName = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_CONDITION_TEXT + 0: return EESEWrite(p[2], i[3], static_cast<EESMCondition*>(p[0])->mChatText);
    case EE_SCREENS_SEETX_CONDITION_TEXT + 1: return EESEWrite(p[2], i[3], static_cast<EESMCondition*>(p[0])->mText);
    case EE_SCREENS_SEETX_CONDITION_TEXT + 2: return EESEWrite(p[2], i[3], static_cast<EESMCondition*>(p[0])->mName);
    case EE_SCREENS_SEETX_SET_CONDITION_TEXT + 0:
        static_cast<EESMCondition*>(p[0])->mChatText = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_CONDITION_TEXT + 1:
        static_cast<EESMCondition*>(p[0])->mText = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_CONDITION_TEXT + 2:
        static_cast<EESMCondition*>(p[0])->mName = SeetBytes(p[1], i[0]);
        return 0;
    case EE_SCREENS_SEETX_SET_OBJECTS:
        static_cast<EESMEffect*>(p[0])->mObjects.assign(static_cast<const uint32_t*>(p[1]), static_cast<const uint32_t*>(p[1]) + i[0]);
        return 0;
    case EE_SCREENS_SEETX_SM_SORT:
        gWorld.mScenarioManager->SortList((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEETX_SM_MOVE:
        gWorld.mScenarioManager->MoveEntry((unsigned long)i[0], (unsigned long)(uint32_t)i[1], i[2] != 0);
        return 0;
    case EE_SCREENS_SEETX_SM_DELETE:
        gWorld.mScenarioManager->DeleteEntry((unsigned long)i[0], (unsigned long)(uint32_t)i[1]);
        return 0;
    case EE_SCREENS_SEETX_SM_DUPLICATE:
        gWorld.mScenarioManager->DuplicateEntry((unsigned long)i[0], (unsigned long)(uint32_t)i[1]);
        return 0;
    case EE_SCREENS_SEETX_SM_NEW:
        gWorld.mScenarioManager->NewEntry((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEETX_SHELL_OP: return (int32_t)EEUIShell::Op(static_cast<UIControl*>(p[0]), (uint32_t)i[0], (uint32_t)i[1]);
    case EE_SCREENS_SEETX_CLASS_CATEGORY: return (int32_t)gUnitClassTable[i[0]]->mCategory;
    case EE_SCREENS_SEETX_CLASS_SELECTABLE_AT: return gUnitClassTable[i[0]]->mIsScenarioSelectable == true;
    case EE_SCREENS_SEETX_CLASS_COUNT: return (int32_t)EEUnitClassCount();
    }
    return 0;
}
