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
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
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

#include "lle/UIRadioButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
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
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESMCondition.h"
#include "ee/EESEARust.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EECPBehavior.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UIESliderChange.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"
// The screen is whole in Rust (ee_screens editor_subscreens.rs) with its seven effect types (editor_effects.rs): the
// constructor (0x582629), the destructor (0x5829a4), ChangeEffectAction (0x582a3b), ChangeEffectType (0x582a69),
// Sort, MoveDown, MoveUp, Delete, Duplicate, Activate (0x582b22), Store (0x582b96), GetNames, Load (0x582c55), New,
// RefreshControls, Show and Hide are its slots; the class forwards.

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// ------------------------------------------------------------------------------------------------ the types' host

void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox);

// Fills a scenario combo box with the area or object names and titles its label 'Area' or 'Object' (once
// EESEEvents.cpp's)
void FillScenarioComboBox(UIComboBox* inComboBox, UITextLabel* inLabel, bool inAreas)
{
    EEUIShell::Op(inComboBox, EE_SCREENS_SKOP_SET_MODE, inAreas ? kSETriggerScreenArea : kSETriggerScreenObject);
    FillPlaceComboBox(inAreas, inLabel, inComboBox);
}

// Switches a scenario combo to areas or objects: sets its label, fills it with the names and selects the first silently
// (once EESETSETPlayer.cpp; the camera and flare types' loads and their radios' event)
void FillAreaObjectComboBox(UITextLabel* inLabel, UIComboBox* inComboBox, bool inAreas)
{
    FillScenarioComboBox(inComboBox, inLabel, inAreas);
    UIEvent* theAction = inComboBox->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inComboBox->SetCurrentSelection(0);
    if (theAction) {
        inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// Sets the place label to 'Area' or 'Object' and fills the place combo with the scenario's area or object names
// (once EESETSETGame.cpp; the place radios' event, ee_screens scenario_editor.rs, calls it through EESEEvents.cpp)
void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox)
{
    std::vector<UWideString> theNames;
    UWideString theTitle;
    if (inAreas) {
        gResource->GetString(IDS_AREA, theTitle);
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_AREAS, theNames);
    } else {
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theTitle);
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_OBJECTS, theNames);
    }
    inLabel->SetText(theTitle);
    inComboBox->DeleteAllEntries();
    for (unsigned long i = 0; i < theNames.size(); i++)
        inComboBox->AppendLine(theNames[i]);
}

namespace {

static_assert(kSETriggerScreenTrigger == 0 && kSETriggerScreenObject == 3 && kSETriggerScreenArea == 4 &&
                  kSETSEffectTitleLabel == 0x960 && kSETSEffectTypeComboBox == 0x961 &&
                  kSETriggerScreenNameEditBox == 0x7db && kSETriggerScreenNameLabel == 0x7dc &&
                  kSETriggerScreenDuplicateButton == 0x7d6 && kSETriggerScreenDeleteButton == 0x7d7 &&
                  kControlTriggerRangeChanged == 0x3e8 && kControlTriggerChecked == 0x3e9 &&
                  kControlTriggerSelectionChanged == 0x3eb && kControlTriggerButtonUp == 0x3f0 &&
                  kTextAlignLeft == 0 && kTextAlignCenter == 1 && kTextAlignRight == 2 &&
                  kSMEffectNone == 0 && kSMEffectObject == 1 && kSMEffectPlayer == 2 && kSMEffectTrigger == 3 &&
                  kSMEffectMedia == 4 && kSMEffectGame == 5 && kSMEffectAI == 6,
              "editor_effects.rs's values");
static_assert(kSMConditionAlwaysTrue == 0 && kSMConditionPlayerAttribute == 2 && kSMConditionPlayerTechnology == 3 &&
                  kSMConditionTriggerAttribute == 4 && kSMConditionPlayerDiplomacy == 5 &&
                  kSMConditionGameAttribute == 6 && kSMConditionChatMessageContains == 7 &&
                  kSETSConditionTitleLabel == 0x8fc && kSETSConditionTypeComboBox == 0x8fd &&
                  kControlTriggerUnchecked == 0x3ea && kControlTriggerEnterPressed == 0x3f1 &&
                  kSEECheckDisplay == EE_SCREENS_SEE_CHECK_DISPLAY,
              "editor_conditions.rs's values");

// a technology's name in the editor (as EESETSConditionType.cpp's EESETSCTResearchNameLess; once EESETSETPlayer.cpp)
UWideString SeetTechName(long inID)
{
    UWideString theName;
    EETechTreeNode* theNode = gTechTree.Get(inID);
    if (theNode) {
        RTSTechTreeNodeData* theData = theNode->mData;
        if (theData->mUnitClass >= 0 && gUnitClassTable[theData->mUnitClass])
            gResource->GetString(gUnitClassTable[theData->mUnitClass]->mNameStringID, theName);
        else
            gResource->GetString(theData->mNameID, theName);
    } else
        theName = "";
    return theName;
}

struct SeetTechNameLess {
    bool operator()(const long& inLeft, const long& inRight) const { return SeetTechName(inLeft) < SeetTechName(inRight); }
};

}   // namespace

// TEST ONLY: the alignment each label was last given through the host (the editor gate's `align`; the toolkit's label
// keeps it without a getter)
std::map<const void*, unsigned long> gSeetAlignments;

namespace {

std::vector<UWideString> sSeetNames;   // EE_SCREENS_SEET_OP_NAMES' kept names
std::vector<long> sSeetTechs;          // EE_SCREENS_SEET_OP_TECHS' kept technologies

int32_t SeetWrite(void* outBuffer, int32_t inCapacity, const char* inText)
{
    size_t theLength = strlen(inText);
    memcpy(outBuffer, inText, (std::min)(theLength, (size_t)(inCapacity > 0 ? inCapacity : 0)));
    return (int32_t)theLength;
}

UWideString SeetBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}

// the controls by array (EE_SCREENS_SEET_A_*): the effect screen's and the condition screen's records are Rust's
// (ee_screens editor_direct.rs: made, freed, their slots and the toolkit's ops); the cases below read a member
// through it
static_assert(sizeof(EESETSEffectControls) == EE_SCREENS_SEET_EFFECT_SLOTS * sizeof(void*) &&
                  offsetof(EESETSEffectControls, mComboBoxes) == 19 * sizeof(void*) &&
                  offsetof(EESETSEffectControls, mListBoxes) == 44 * sizeof(void*),
              "editor_direct.rs's effect record");
static_assert(sizeof(EESETSConditionControls) == EE_SCREENS_SEET_CONDITION_SLOTS * sizeof(void*) &&
                  offsetof(EESETSConditionControls, mCheckBoxes) == 15 * sizeof(void*) &&
                  offsetof(EESETSConditionControls, mSlider) == 22 * sizeof(void*),
              "editor_direct.rs's condition record");

struct SeetC {
    void* mControls;
    bool mCondition;
    template <class T = UIControl>
    T* At(int32_t a, int32_t k) const
    {
        return static_cast<T*>(static_cast<UIControl*>(ee_screens_seet_slot(mControls, mCondition, (uint32_t)a, (uint32_t)k)));
    }
    UITextLabel* Label(int32_t k) const { return At<UITextLabel>(EE_SCREENS_SEET_A_LABEL, k); }
    UIComboBox* Combo(int32_t a, int32_t k) const
    {
        bool theCombo = a == EE_SCREENS_SEET_A_COMBO || a == EE_SCREENS_SEET_A_ECOMBO || a == EE_SCREENS_SEET_A_SCEN ||
                        a == EE_SCREENS_SEET_A_PLAYER;
        return theCombo ? At<UIComboBox>(a, k) : NULL;
    }
    UIEditBox* Edit(int32_t a, int32_t k) const
    {
        bool theEdit = a == EE_SCREENS_SEET_A_EDIT || a == EE_SCREENS_SEET_A_EDIT2 || a == EE_SCREENS_SEET_A_TEXT;
        return theEdit ? At<UIEditBox>(a, k) : NULL;
    }
    UILBString* List(int32_t k) const { return At<UILBString>(EE_SCREENS_SEET_A_LIST, k); }
    UISlider* Slider(int32_t k) const { return At<UISlider>(EE_SCREENS_SEET_A_SLIDER, k); }
};

// The ops both hosts share that the toolkit's Rust does not answer (editor_effects.rs: p0 the controls, p1 the
// entry or a handle, p2 a buffer or an out word; i0 the control array, i1 its index, i2 and i3 the arguments; f the
// floats)
int32_t SeetOp(uint32_t inOp, const SeetC& c, void* const* p, int32_t* i, float* f)
{
    int32_t a = i[0];
    int32_t k = i[1];
    UWideString theText;
    switch (inOp) {
    case EE_SCREENS_SEET_OP_SET_RECT:
        c.At(a, k)->SetRectangle(GERectangle(GE2DPoint(f[0], f[1]), GE2DPoint(f[2], f[3])));
        return 0;
    case EE_SCREENS_SEET_OP_ALIGN:
        c.Label(k)->SetAlignment((unsigned long)i[2]);
        gSeetAlignments[c.Label(k)] = (unsigned long)i[2];
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_PUSH:
        EESEEPushControl(static_cast<EESEERust*>(static_cast<UIEvent*>(p[1])), c.At(a, k));
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_PROCESS:
        static_cast<UIEvent*>(p[1])->ProcessEvent(gUIManager->GetScenarioEditorForm(), c.At(a, k));
        return 0;
    case EE_SCREENS_SEET_OP_CHECK: c.At<UIRadioButton>(EE_SCREENS_SEET_A_RADIO, k)->Check(); return 0;
    case EE_SCREENS_SEET_OP_TEXT_FONT: {
        GEFont* theFont = i[3] == EE_SCREENS_SEPL_FONT_538 ? gU531938 : gU531934;
        gResource->GetString((unsigned long)i[2], theText);
        if (a == EE_SCREENS_SEET_A_RADIO) c.At<UIRadioButton>(a, k)->SetText(theText, theFont);
        else c.At<UICheckBox>(EE_SCREENS_SEET_A_CHECK, k)->SetText(theText, theFont);
        return 0;
    }
    case EE_SCREENS_SEET_OP_YELLOW: {
        GEText* theGEText = a == EE_SCREENS_SEET_A_CHECK   ? c.At<UICheckBox>(a, k)->GetTextLabel()->GetGEText()
                            : a == EE_SCREENS_SEET_A_RADIO ? c.At<UIRadioButton>(a, k)->GetTextLabel()->GetGEText()
                                                           : c.List(k)->GetGEText();
        theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theGEText->SetIsTransparent(true);
        return 0;
    }
    case EE_SCREENS_SEET_OP_FONT_530:
        if (UIEditBox* e = c.Edit(a, k)) e->GetGEText()->SetFont(gU531930);
        else {
            c.Combo(a, k)->GetGEText()->SetFont(gU531930);
            if (i[2] != 0)
                c.Combo(a, k)->GetListBox()->GetGEText()->SetFont(gU531930);
        }
        return 0;
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
    case EE_SCREENS_SEET_OP_FILL_NAMES: {
        std::vector<UWideString> theNames;
        UIComboBox* theCombo = c.Combo(a, k);
        gWorld.mScenarioManager->GetNames((unsigned long)i[2], theNames);
        if (i[3] != 0)
            theCombo->AppendStringArray(theNames);
        else
            for (unsigned long n = 0; n < theNames.size(); n++)
                theCombo->AppendLine(theNames[n]);
        return 0;
    }
    case EE_SCREENS_SEET_OP_FILL_AREA_OBJECT:
        FillAreaObjectComboBox(c.Label(i[2]), c.At<UIComboBox>(EE_SCREENS_SEET_A_SCEN, k), i[3] != 0);
        return 0;
    case EE_SCREENS_SEET_OP_FILL_SCENARIO:
        FillScenarioComboBox(c.At<UIComboBox>(EE_SCREENS_SEET_A_SCEN, k), c.Label(i[2]), i[3] != 0);
        return 0;
    case EE_SCREENS_SEET_OP_FILL_PLACE:
        FillPlaceComboBox(i[3] != 0, c.Label(i[2]), c.Combo(EE_SCREENS_SEET_A_COMBO, k));
        return 0;
    case EE_SCREENS_SEET_OP_LIST_OPACITY:
        c.List(k)->SetBgOpacity(f[0]);
        c.List(k)->SetScrollBarBgOpacity(f[0]);
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
    case EE_SCREENS_SEET_OP_EVENT_NEW: {
        UIEvent* theEvent = NULL;
        switch (i[0]) {
        case EE_SCREENS_SEET_EV_SCREEN: theEvent = EECreateScreenEvent((uint32_t)i[1]); break;
        case EE_SCREENS_SEET_EV_SEE: theEvent = new EESEERust((uint32_t)i[1]); break;
        case EE_SCREENS_SEET_EV_SLIDER: theEvent = new UIESliderChange(c.Label(i[1]), f[0], (long)i[2]); break;
        case EE_SCREENS_SEET_EV_SLIDER_FORMAT:
            theEvent = EESEENewSliderChange(c.Label(i[1]), i[2] != 0 ? "%#2.1f" : "%2.0f", f[0], f[1]);
            break;
        case EE_SCREENS_SEET_EV_USE_OBJECT_AREA:
            theEvent = EESEENewUseObjectArea(c.Label(i[1]), c.At<UIComboBox>(EE_SCREENS_SEET_A_SCEN, i[2]),
                                              c.At(EE_SCREENS_SEET_A_RADIO, i[3]));
            break;
        case EE_SCREENS_SEET_EV_OBJECTS: theEvent = EESEENewObjects((uint32_t)i[1], p[1], c.mControls); break;
        }
        *static_cast<void**>(p[2]) = theEvent;
        return 0;
    }
    case EE_SCREENS_SEET_OP_EVENT_STRING:
        if (i[3] != 0)
            gResource->GetString((unsigned long)i[2], theText);
        else
            theText = SeetBytes(p[2], i[2]);
        static_cast<UIESliderChange*>(static_cast<UIEvent*>(p[1]))->AddString(f[0], theText);
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_RELEASE: static_cast<UIEvent*>(p[1])->Dereference(); return 0;
    case EE_SCREENS_SEET_OP_STRING:
        gResource->GetString((unsigned long)i[2], theText);
        return SeetWrite(p[2], k, theText);
    case EE_SCREENS_SEET_OP_FORMAT_NUMBER: {
        UWideString theFormat;
        gResource->GetString((unsigned long)i[2], theFormat);
        sprintf(theText, theFormat, (unsigned long)(uint32_t)i[3]);
        return SeetWrite(p[2], k, theText);
    }
    case EE_SCREENS_SEET_OP_FORMAT_FLOAT:
        sprintf(theText, static_cast<const char*>(p[1]), (double)f[0]);
        return SeetWrite(p[2], k, theText);
    case EE_SCREENS_SEET_OP_SET_NAME_BOX:
        gUIManager->GetScenarioEditorForm()->GetTriggerBox(false)->SetText(SeetBytes(p[2], i[2]));
        return 0;
    case EE_SCREENS_SEET_OP_NAME_BOX:
        return SeetWrite(p[2], k, gUIManager->GetScenarioEditorForm()->GetTriggerBox(false)->GetText(0));
    case EE_SCREENS_SEET_OP_NAMES:
        sSeetNames.clear();
        gWorld.mScenarioManager->GetNames((unsigned long)i[2], sSeetNames);
        return (int32_t)sSeetNames.size();
    case EE_SCREENS_SEET_OP_NAME: return SeetWrite(p[2], k, sSeetNames[(size_t)i[2]]);
    case EE_SCREENS_SEET_OP_OBJECT_NAME: {
        EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject((unsigned long)(uint32_t)i[2]);
        return theObject != NULL ? SeetWrite(p[2], k, theObject->mName) : -1;
    }
    case EE_SCREENS_SEET_OP_TRIGGER_NAME: {
        EESMTrigger* theTrigger = gWorld.mScenarioManager->GetTrigger((unsigned long)(uint32_t)i[2]);
        return theTrigger != NULL ? SeetWrite(p[2], k, theTrigger->mName) : -1;
    }
    case EE_SCREENS_SEET_OP_CALAMITY_COUNT: return (int32_t)EECalamityManager::GetInstance()->GetCalamityClassCount();
    case EE_SCREENS_SEET_OP_CALAMITY: {
        EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass((unsigned long)i[2]);
        if (!theClass)
            return 0;
        i[2] = (int32_t)theClass->mID;
        i[3] = (int32_t)theClass->mNameID;
        return 1;
    }
    case EE_SCREENS_SEET_OP_GRAPHIC_COUNT: return (int32_t)EEGraphicEffect::GetCount();
    case EE_SCREENS_SEET_OP_GRAPHIC: {
        EEGraphicEffect* theGraphic = EEGraphicEffect::Get((unsigned long)i[2]);
        if (!theGraphic || theGraphic->mIsScenarioListed != true)
            return 0;
        i[2] = (int32_t)theGraphic->mID;
        i[3] = (int32_t)theGraphic->mNameStringId;
        return 1;
    }
    case EE_SCREENS_SEET_OP_BEHAVIOR_COUNT: return (int32_t)(sizeof(gCPBehaviors) / sizeof(EECPBehavior));
    case EE_SCREENS_SEET_OP_BEHAVIOR: {
        const EECPBehavior& b = gCPBehaviors[i[2]];
        switch (i[3]) {
        case EE_SCREENS_SEET_B_SETTABLE: return b.mIsSettable == true ? 1 : 0;
        case EE_SCREENS_SEET_B_INT1_MIN: return b.mIntValue1Min;
        case EE_SCREENS_SEET_B_INT1_MAX: return b.mIntValue1Max;
        case EE_SCREENS_SEET_B_INT2_MIN: return b.mIntValue2Min;
        case EE_SCREENS_SEET_B_INT2_MAX: return b.mIntValue2Max;
        case EE_SCREENS_SEET_B_FLOAT_MIN: return b.mFloatSliderMin;
        case EE_SCREENS_SEET_B_FLOAT_MAX: return b.mFloatSliderMax;
        default: return b.mFloatSliderScale;
        }
    }
    case EE_SCREENS_SEET_OP_UNIT_TYPE_COUNT: return (int32_t)EEUnitClass::GetUnitTypes().size();
    case EE_SCREENS_SEET_OP_UNIT_TYPE: {
        EEUnitClass* theClass = gUnitClassTable[EEUnitClass::GetUnitTypes()[(size_t)i[2]]];
        if (theClass->mIsScenarioSelectable != true)
            return -1;
        i[2] = (int32_t)theClass->mUnitType;
        if ((theClass->mDomains & kMovementAir) == kMovementAir)
            return EE_SCREENS_SEET_D_AIR;
        if ((theClass->mDomains & kMovementSea) == kMovementSea)
            return EE_SCREENS_SEET_D_SEA;
        return EE_SCREENS_SEET_D_LAND;
    }
    case EE_SCREENS_SEET_OP_UNIT_NAME: return (int32_t)gUnitClassTable[i[2]]->mNameStringID;
    case EE_SCREENS_SEET_OP_TECHS: {
        // the tree's nodes and the epochs, those shown in the editor (i2: and making no unit, the condition
        // types'), sorted by name (the originals' first Show)
        sSeetTechs.clear();
        gTechTree.GetNodeIDs(sSeetTechs);
        for (unsigned long theEpoch = 0; theEpoch < (unsigned long)EEEpochCount(); theEpoch++)
            sSeetTechs.push_back((long)theEpoch);
        unsigned long theIndex = 0;
        while (theIndex < sSeetTechs.size()) {
            RTSTechTreeNodeData* theData = gTechTree.Get(sSeetTechs[theIndex])->mData;
            if (theData->mShowInEditor == true && (i[2] == 0 || theData->mMakesUnit == false))
                theIndex++;
            else
                sSeetTechs.erase(sSeetTechs.begin() + theIndex);
        }
        std::sort(sSeetTechs.begin(), sSeetTechs.end(), SeetTechNameLess());
        return (int32_t)sSeetTechs.size();
    }
    case EE_SCREENS_SEET_OP_TECH: return (int32_t)sSeetTechs[(size_t)i[2]];
    case EE_SCREENS_SEET_OP_TECH_LINE: {
        EETechTreeNode* theNode = gTechTree.Get(i[2]);
        theText = SeetTechName(i[2]);
        if (theNode && theText == "FONTS")
            sprintf(theText, "BAD Node %ld - (name not available in release)", theNode->mData->mID);
        return SeetWrite(p[2], k, theText);
    }
    case EE_SCREENS_SEET_OP_TECH_NAME: return SeetWrite(p[2], k, SeetTechName(i[2]));
    case EE_SCREENS_SEET_OP_TSCREEN: {
        EEUIFScenarioEditor* theScreen = gUIManager->GetScenarioEditorForm();
        theScreen->TriggerCall(i[2] == EE_SCREENS_SEET_TS_HIDE_TEXT          ? EE_SCREENS_SETS_HIDE_TEXT
                               : i[2] == EE_SCREENS_SEET_TS_SHOW_ALPHA_SORT ? EE_SCREENS_SETS_SHOW_ALPHA_SORT
                                                                            : EE_SCREENS_SETS_SHOW_TEXT);
        return 0;
    }
    case EE_SCREENS_SEET_OP_TEXT_BOX:
        gUIManager->GetScenarioEditorForm()->GetTriggerBox(true)->GetAllText(theText);
        return SeetWrite(p[2], k, theText);
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

// the scenario's list inList's operation inOp (EE_SCREENS_SEOB_SM_*) on the entry inEntry (the screens' list buttons)
void SeetEntryList(unsigned long inList, int32_t inOp, int32_t inEntry)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    unsigned long theEntry = (unsigned long)(uint32_t)inEntry;
    switch (inOp) {
    case EE_SCREENS_SEOB_SM_SORT: theManager->SortList(inList); break;
    case EE_SCREENS_SEOB_SM_DOWN: theManager->MoveEntry(inList, theEntry, false); break;
    case EE_SCREENS_SEOB_SM_UP: theManager->MoveEntry(inList, theEntry, true); break;
    case EE_SCREENS_SEOB_SM_DELETE: theManager->DeleteEntry(inList, theEntry); break;
    case EE_SCREENS_SEOB_SM_DUPLICATE: theManager->DuplicateEntry(inList, theEntry); break;
    default: theManager->NewEntry(inList); break;
    }
}

// The effect screen's and types' host (editor_subscreens.rs, editor_effects.rs): the shared ops over the effect
// screen's controls (editor_direct.rs answers the toolkit's first); the effect, its texts and objects, the effects' list
extern "C" int32_t ee_screenscpp_seet(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESMEffect* theEffect = static_cast<EESMEffect*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEET_OP_ENTRY: {
        EESMEffect* theEntry = gWorld.mScenarioManager->GetEffect((unsigned long)(uint32_t)i[2]);
        static_cast<void**>(p[2])[0] = theEntry;
        static_cast<void**>(p[2])[1] = theEntry != NULL ? static_cast<EeScenarioEffectFields*>(theEntry) : NULL;
        return 0;
    }
    case EE_SCREENS_SEET_OP_LIST: SeetEntryList(EE_SCENARIO_LIST_EFFECTS, i[2], i[3]); return 0;
    case EE_SCREENS_SEET_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_EFFECTS, *static_cast<std::vector<UWideString>*>(p[1]));
        return 0;
    case EE_SCREENS_SEET_OP_EFFECT_TEXT:
    case EE_SCREENS_SEET_OP_SET_EFFECT_TEXT: {
        UWideString* theField = i[2] == EE_SCREENS_SEET_S_NEW_NAME      ? &theEffect->mNewName
                                : i[2] == EE_SCREENS_SEET_S_MESSAGE     ? &theEffect->mMessage
                                : i[2] == EE_SCREENS_SEET_S_FILE_NAME   ? &theEffect->mFileName
                                : i[2] == EE_SCREENS_SEET_S_DESCRIPTION ? &theEffect->mDescription
                                : i[2] == EE_SCREENS_SEET_S_TEXT        ? &theEffect->mText
                                                                        : &theEffect->mName;
        if (inOp == EE_SCREENS_SEET_OP_EFFECT_TEXT)
            return SeetWrite(p[2], i[1], *theField);
        *theField = SeetBytes(p[2], i[3]);
        return 0;
    }
    case EE_SCREENS_SEET_OP_OBJECT_COUNT: return (int32_t)theEffect->mObjects.size();
    case EE_SCREENS_SEET_OP_OBJECT: return (int32_t)theEffect->mObjects[(size_t)i[2]];
    case EE_SCREENS_SEET_OP_SET_OBJECTS: {
        const uint32_t* theObjects = static_cast<const uint32_t*>(p[2]);
        theEffect->mObjects.assign(theObjects, theObjects + (i[2] > 0 ? i[2] : 0));
        return 0;
    }
    }
    return SeetOp(inOp, SeetC{p[0], false}, p, i, f);
}

// The condition screen's and types' host (editor_subscreens.rs, editor_conditions.rs): the shared ops over the
// condition screen's controls (editor_direct.rs answers the toolkit's first); the condition, its texts, its list
extern "C" int32_t ee_screenscpp_sect(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESMCondition* theCondition = static_cast<EESMCondition*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEET_OP_ENTRY: {
        EESMCondition* theEntry = gWorld.mScenarioManager->GetCondition((unsigned long)(uint32_t)i[2]);
        static_cast<void**>(p[2])[0] = theEntry;
        static_cast<void**>(p[2])[1] = theEntry != NULL ? static_cast<EeScenarioConditionFields*>(theEntry) : NULL;
        return 0;
    }
    case EE_SCREENS_SEET_OP_LIST: SeetEntryList(EE_SCENARIO_LIST_CONDITIONS, i[2], i[3]); return 0;
    case EE_SCREENS_SEET_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_CONDITIONS, *static_cast<std::vector<UWideString>*>(p[1]));
        return 0;
    case EE_SCREENS_SEET_OP_EFFECT_TEXT:
    case EE_SCREENS_SEET_OP_SET_EFFECT_TEXT: {
        UWideString* theField = i[2] == EE_SCREENS_SEET_S_CHAT   ? &theCondition->mChatText
                                : i[2] == EE_SCREENS_SEET_S_TEXT ? &theCondition->mText
                                                                 : &theCondition->mName;
        if (inOp == EE_SCREENS_SEET_OP_EFFECT_TEXT)
            return SeetWrite(p[2], i[1], *theField);
        *theField = SeetBytes(p[2], i[3]);
        return 0;
    }
    }
    return SeetOp(inOp, SeetC{p[0], true}, p, i, f);
}
