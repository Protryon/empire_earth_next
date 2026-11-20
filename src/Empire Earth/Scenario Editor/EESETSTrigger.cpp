// C:\EEX\Empire Earth\Scenario Editor\EESETSTrigger.cpp (file name inferred: no __FILE__; RTTI class EESETSTrigger).
// The trigger screen's trigger sub-screen: 0x19393b..0x196ebe (ctor, its combo box class's inline ctor and
// MouseRButtonUp, the ??_G pair, the dtor, the slots and members in vtable order of their bodies, the trigger's
// GetConditionCount COMDAT, then the trigger events' ??_G). After EESETSObject.cpp (whose range ends with
// vector<UWideString>::operator= 0x1937b1), before EESETSUICBPlayer.cpp (0x196ebe). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
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
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/USingleton.h"
#include "ee/EEMedia.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESMCondition.h"
#include "ee/EESMTrigger.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"

// Opens the trigger screen's tab of the entry right-clicked in a combo box (this one and the effects' scenario combo
// boxes, EESETSEScenarioComboBox): which tab and entry is ee_screens editor_trigger.rs' (the condition list's first
// object condition sends the object conditions to the object tab); the tab is checked without its event
void EESETriggerJump(unsigned long inKind, unsigned long inSelection)
{
    EeScreensSeTriggerTab* theScreen = gUIManager->GetScenarioEditorForm()->GetTriggerTab();
    unsigned long theCount = gWorld.mScenarioManager->mConditions.size();
    unsigned long theObject = 0;
    while (theObject < theCount && gWorld.mScenarioManager->GetCondition(theObject)->GetType() != kSMConditionObject)
        theObject++;
    int64_t theJump = ee_screens_trigger_jump(inKind, inSelection, theObject, theCount);
    if (theJump < 0)
        return;
    long theTab = (long)(theJump >> 32);
    UIRadioButton* theButton = static_cast<UIRadioButton*>(ee_screens_sets_control(theScreen, EE_SCREENS_SETS_TAB, (uint32_t)theTab));
    UIEvent* theEvent = theButton->GetTriggerEvent(kControlTriggerChecked);
    if (theEvent != NULL)
        theEvent->Reference();
    theButton->SetTriggerEvent(kControlTriggerChecked, NULL);
    theButton->Check();
    theButton->SetTriggerEvent(kControlTriggerChecked, theEvent);
    ee_screens_sets_sub_select(theScreen, (uint32_t)theTab, (int32_t)(uint32_t)theJump);
    ee_screens_sets_call(theScreen, EE_SCREENS_SETS_SHOW_TAB, (uint32_t)(kSETriggerTabTrigger + theTab), 0.0f, 0.0f, NULL);
}

// The trigger screen whole in Rust (ee_screens editor_trigger.rs): the constructor (0x59393b), the destructor
// (0x595fde), Sort, MoveDown, MoveUp, Delete, UpdateSecondConditionRow, UpdateSecondEffectRow, Duplicate, Activate,
// Store (0x596415), ChangeOperator, GetNames, Hide, Load (0x596785), New, RemoveEntry, ShowOperator, Show and the
// rows' steps are its members; the class forwards. EESETSTriggerUIComboBox stays C++ (above), and this unit is the
// screen's host (ee_screenscpp_setr).

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// ---- the host (editor_trigger.rs: the ops from 200)

static_assert(EE_SCREENS_SETR_IDS_SE_REMOVE == IDS_SE_REMOVE && EE_SCREENS_SETR_IDS_CONDITION == IDS_CONDITION &&
                  EE_SCREENS_SETR_IDS_EFFECT == IDS_EFFECT && EE_SCREENS_SETR_IDS_SECONDS == IDS_SECONDS &&
                  EE_SCREENS_SETR_IDS_IF == IDS_IF && EE_SCREENS_SETR_IDS_IF + 1 == IDS_THEN &&
                  EE_SCREENS_SETR_IDS_IF + 2 == IDS_NOT && EE_SCREENS_SETR_IDS_IF + 3 == IDS_DELAY &&
                  EE_SCREENS_SETR_IDS_IF + 4 == IDS_AND && EE_SCREENS_SETR_IDS_IF + 5 == IDS_OR &&
                  EE_SCREENS_SETR_IDS_IF + 6 == IDS_TRIGGER_STATEMENT && EE_SCREENS_SETR_IDS_IF + 7 == IDS_TRIGGER_IS_ON &&
                  EE_SCREENS_SETR_IDS_IF + 8 == IDS_CONDITIONS_TRUE_FOR && EE_SCREENS_SETR_IDS_IF + 9 == IDS_LOOPING &&
                  EE_SCREENS_SETR_IDS_INCLUDE_IN_CINEMATIC == IDS_INCLUDE_IN_CINEMATIC,
              "the strings");
static_assert(EE_SCREENS_SETR_END == kSMTriggerOperatorEnd && EE_SCREENS_SETR_AND == kSMTriggerOperatorAnd &&
                  EE_SCREENS_SETR_OR == kSMTriggerOperatorOr,
              "the operators");

extern "C" int32_t ee_screenscpp_setr(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    (void)f;
    EESMTrigger* t = NULL;
    if (inOp >= EE_SCREENS_SETR_OP_EXISTS && inOp <= EE_SCREENS_SETR_OP_DELETE) {
        t = gWorld.mScenarioManager->GetTrigger((unsigned long)i[0]);
        if (t == NULL)
            return 0;
    }
    auto theScreen = [] { return gUIManager->GetScenarioEditorForm(); };
    switch (inOp) {
    case EE_SCREENS_SETR_OP_SM:
        switch (i[0]) {
        case EE_SCREENS_SEOB_SM_SORT: gWorld.mScenarioManager->SortList(EE_SCENARIO_LIST_TRIGGERS); break;
        case EE_SCREENS_SEOB_SM_DOWN: gWorld.mScenarioManager->MoveEntry(EE_SCENARIO_LIST_TRIGGERS, i[1], false); break;
        case EE_SCREENS_SEOB_SM_UP: gWorld.mScenarioManager->MoveEntry(EE_SCENARIO_LIST_TRIGGERS, i[1], true); break;
        case EE_SCREENS_SEOB_SM_DELETE: gWorld.mScenarioManager->DeleteEntry(EE_SCENARIO_LIST_TRIGGERS, i[1]); break;
        case EE_SCREENS_SEOB_SM_DUPLICATE: gWorld.mScenarioManager->DuplicateEntry(EE_SCENARIO_LIST_TRIGGERS, i[1]); break;
        default: gWorld.mScenarioManager->NewEntry(EE_SCENARIO_LIST_TRIGGERS); break;
        }
        return 0;
    case EE_SCREENS_SETR_OP_APPEND_NAMES: {
        std::vector<UWideString> theNames;
        gWorld.mScenarioManager->GetNames(i[0] == 0 ? EE_SCENARIO_NAMES_CONDITIONS : EE_SCENARIO_NAMES_EFFECTS, theNames);
        for (size_t k = 0; k < theNames.size(); k++) {
            static_cast<UIComboBox*>(p[0])->AppendLine(theNames[k]);
            static_cast<UIComboBox*>(p[1])->AppendLine(theNames[k]);
        }
        return 0;
    }
    case EE_SCREENS_SETR_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_TRIGGERS, *static_cast<std::vector<UWideString>*>(p[0]));
        return 0;
    case EE_SCREENS_SETR_OP_HIDE_TEXT:
        theScreen()->TriggerCall(EE_SCREENS_SETS_HIDE_TEXT);
        theScreen()->TriggerCall(EE_SCREENS_SETS_HIDE_ALPHA_SORT);
        return 0;
    case EE_SCREENS_SETR_OP_SHOW_TEXT: theScreen()->TriggerCall(EE_SCREENS_SETS_SHOW_TEXT); return 0;
    case EE_SCREENS_SETR_OP_EXISTS: return 1;
    case EE_SCREENS_SETR_OP_GET:
        switch (i[1]) {
        case EE_SCREENS_SETR_V_ON: return t->mIsOn;
        case EE_SCREENS_SETR_V_DURATION: return (int32_t)t->mConditionDuration;
        case EE_SCREENS_SETR_V_LOOPING: return t->mIsLooping;
        case EE_SCREENS_SETR_V_CINEMATIC: return t->mIsInCinematic;
        case EE_SCREENS_SETR_V_CONDITIONS: return (int32_t)t->GetConditionCount();
        default: return (int32_t)t->GetEffectCount();
        }
    case EE_SCREENS_SETR_OP_SET:
        switch (i[1]) {
        case EE_SCREENS_SETR_V_ON: t->SetIsOn(i[2] != 0); break;
        case EE_SCREENS_SETR_V_DURATION: t->SetConditionDuration((unsigned long)(long)i[2]); break;
        case EE_SCREENS_SETR_V_LOOPING: t->SetIsLooping(i[2] != 0); break;
        case EE_SCREENS_SETR_V_CINEMATIC: t->SetIsInCinematic(i[2] != 0); break;
        }
        return 0;
    case EE_SCREENS_SETR_OP_TEXTS_FROM_SCREEN: {
        UWideString theText;
        t->SetName(UWideString(theScreen()->GetTriggerBox(false)->GetText(0)));
        theScreen()->GetTriggerBox(true)->GetAllText(theText);
        t->SetText(theText);
        return 0;
    }
    case EE_SCREENS_SETR_OP_TEXTS_TO_SCREEN:
        theScreen()->GetTriggerBox(false)->SetText(t->mName);
        theScreen()->GetTriggerBox(true)->SetText(t->mText);
        return 0;
    case EE_SCREENS_SETR_OP_ENTRY:
        if (i[1] == 0) {
            EESMTConditionWrapper* c = t->GetCondition((unsigned long)i[2]);
            if (c == NULL)
                return 0;
            switch (i[3]) {
            case EE_SCREENS_SETR_W_FIRST: return c->mIsNot;
            case EE_SCREENS_SETR_W_VALUE: return (int32_t)c->mValue;
            case EE_SCREENS_SETR_W_OPERATOR: return (int32_t)c->mOperator;
            default: return 1;
            }
        } else {
            EESMTEffectWrapper* e = t->GetEffect((unsigned long)i[2]);
            if (e == NULL)
                return 0;
            switch (i[3]) {
            case EE_SCREENS_SETR_W_FIRST: return (int32_t)e->mDelay;
            case EE_SCREENS_SETR_W_VALUE: return (int32_t)e->mEffectIndex;
            case EE_SCREENS_SETR_W_OPERATOR: return (int32_t)e->mOperator;
            default: return 1;
            }
        }
    case EE_SCREENS_SETR_OP_SET_CONDITION: t->SetCondition((unsigned long)i[1], i[2] != 0, (unsigned long)i[3]); return 0;
    case EE_SCREENS_SETR_OP_SET_EFFECT: t->SetEffect((unsigned long)i[1], (long)i[2], (unsigned long)i[3]); return 0;
    case EE_SCREENS_SETR_OP_SET_OPERATOR:
        if (i[1] == 0)
            t->SetConditionOperator((unsigned long)i[2], (long)i[3]);
        else
            t->SetEffectOperator((unsigned long)i[2], (long)i[3]);
        return 0;
    case EE_SCREENS_SETR_OP_DELETE:
        if (i[1] == 0)
            t->DeleteCondition((unsigned long)i[2]);
        else
            t->DeleteEffect((unsigned long)i[2]);
        return 0;
    case EE_SCREENS_SETR_OP_NEW_EVENT:
        *static_cast<UIEvent**>(p[0]) = EECreateScreenEvent(EE_SCREENS_SE_TRIGGER_OPERATOR);
        return 0;
    case EE_SCREENS_SETR_OP_RELEASE_EVENT: static_cast<UIEvent*>(p[0])->Dereference(); return 0;
    case EE_SCREENS_SETR_OP_REMOVE_TEXT: {
        UWideString theFormat, theText;
        gResource->GetString(IDS_DEL, theFormat);
        sprintf(theText, "%s %s", (const char*)theFormat, (const char*)static_cast<UITextLabel*>(p[1])->GetText(0));
        static_cast<UIButton*>(p[0])->SetText(theText, gU531934);
        return 0;
    }
    case EE_SCREENS_SETR_OP_END_TEXT: {
        UWideString theText;
        gResource->GetString(IDS_TRIGGER_EDITOR_END, theText);
        static_cast<UITextLabel*>(static_cast<UIComboBox*>(p[0])->GetTextLabel())->SetText(theText);
        return 0;
    }
    }
    return 0;
}
