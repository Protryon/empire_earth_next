// C:\EEX\Empire Earth\Scenario Editor\EESETSTrigger.cpp (file name inferred: no __FILE__; RTTI class EESETSTrigger).
// The trigger screen's trigger sub-screen: 0x19393b..0x196ebe (ctor, its combo box class's inline ctor and
// MouseRButtonUp, the ??_G pair, the dtor, the slots and members in vtable order of their bodies, the trigger's
// GetConditionCount COMDAT, then the trigger events' ??_G). After EESETSObject.cpp (whose range ends with
// vector<UWideString>::operator= 0x1937b1), before EESETSUICBPlayer.cpp (0x196ebe). Unit flags: the exe module set.
#include <windows.h>
#include <string>
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
// object condition sends the object conditions to the object tab); the tab is checked without its event (the
// sequence: editor_trigx.rs trigger_jump)
void EESETriggerJump(unsigned long inKind, unsigned long inSelection)
{
    ee_screens_trgx_trigger_jump(inKind, inSelection);
}

// The trigger screen whole in Rust (ee_screens editor_trigger.rs): the constructor (0x59393b), the destructor
// (0x595fde), Sort, MoveDown, MoveUp, Delete, UpdateSecondConditionRow, UpdateSecondEffectRow, Duplicate, Activate,
// Store (0x596415), ChangeOperator, GetNames, Hide, Load (0x596785), New, RemoveEntry, ShowOperator, Show and the
// rows' steps are its members; the class forwards. EESETSTriggerUIComboBox stays C++ (above), and this unit is the
// screen's host (ee_screenscpp_setr).


// ---- the host (editor_trigger.rs: the ops from 200)


// the screen's own ops (editor_trigx.rs composes the sequences and the ops on a trigger)
extern "C" int32_t ee_screenscpp_setr(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SETR_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_TRIGGERS, *static_cast<std::vector<UWideString>*>(p[0]));
        return 0;
    case EE_SCREENS_SETR_OP_NEW_EVENT:
        *static_cast<UIEvent**>(p[0]) = EECreateScreenEvent(EE_SCREENS_SE_TRIGGER_OPERATOR);
        return 0;
    case EE_SCREENS_SETR_OP_RELEASE_EVENT: static_cast<UIEvent*>(p[0])->Dereference(); return 0;
    }
    return 0;
}

namespace {

UWideString SetrBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}


UWideString sSetrText;   // the kept remove button's text (EE_SCREENS_SETRX_FORMAT_REMOVE)

}   // namespace

// the screen's single statements (editor_trigx.rs EE_SCREENS_SETRX_*, 300..: p0 the trigger or a control, p2 the out
// word of a found object or a text's buffer (capacity i3), a text p1's i0 bytes)
extern "C" int32_t ee_screenscpp_setrx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[2]);
    EESMTrigger* t = static_cast<EESMTrigger*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SETRX_SET_NAME:
        t->SetName(SetrBytes(p[1], i[0]));
        return 0;
    case EE_SCREENS_SETRX_SET_TEXT:
        t->SetText(SetrBytes(p[1], i[0]));
        return 0;
    case EE_SCREENS_SETRX_NAME: return EESEWrite(p[2], i[3], t->mName);
    case EE_SCREENS_SETRX_TEXT: return EESEWrite(p[2], i[3], t->mText);
    case EE_SCREENS_SETRX_CONDITION:
        *theOut = t->GetCondition((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SETRX_CONDITION_NOT: return static_cast<EESMTConditionWrapper*>(p[0])->mIsNot;
    case EE_SCREENS_SETRX_CONDITION_VALUE: return (int32_t) static_cast<EESMTConditionWrapper*>(p[0])->mValue;
    case EE_SCREENS_SETRX_CONDITION_OPERATOR: return (int32_t) static_cast<EESMTConditionWrapper*>(p[0])->mOperator;
    case EE_SCREENS_SETRX_EFFECT:
        *theOut = t->GetEffect((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SETRX_EFFECT_DELAY: return (int32_t) static_cast<EESMTEffectWrapper*>(p[0])->mDelay;
    case EE_SCREENS_SETRX_EFFECT_INDEX: return (int32_t) static_cast<EESMTEffectWrapper*>(p[0])->mEffectIndex;
    case EE_SCREENS_SETRX_EFFECT_OPERATOR: return (int32_t) static_cast<EESMTEffectWrapper*>(p[0])->mOperator;
    case EE_SCREENS_SETRX_SET_CONDITION_OPERATOR:
        t->SetConditionOperator((unsigned long)i[0], (long)i[1]);
        return 0;
    case EE_SCREENS_SETRX_SET_EFFECT_OPERATOR:
        t->SetEffectOperator((unsigned long)i[0], (long)i[1]);
        return 0;
    case EE_SCREENS_SETRX_DELETE_CONDITION:
        t->DeleteCondition((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SETRX_DELETE_EFFECT:
        t->DeleteEffect((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SETRX_FORMAT_REMOVE:
        sprintf(sSetrText, "%s %s", (const char*)SetrBytes(p[1], i[0]), (const char*)static_cast<UITextLabel*>(p[0])->GetText(0));
        return 0;
    case EE_SCREENS_SETRX_BUTTON_KEPT_TEXT:
        static_cast<UIButton*>(p[0])->SetText(sSetrText, gU531934);
        return 0;
    case EE_SCREENS_SETRX_CONDITION_IS_OBJECT:
        return gWorld.mScenarioManager->GetCondition((unsigned long)(uint32_t)i[0])->GetType() == EE_SCENARIO_SM_CONDITION_OBJECT;
    case EE_SCREENS_SETRX_CONDITION_COUNT: return (int32_t)gWorld.mScenarioManager->mConditions.size();
    case EE_SCREENS_SETRX_TRIGGER_TAB:
        *theOut = gUIManager->GetScenarioEditorForm()->GetTriggerTab();
        return 0;
    case EE_SCREENS_SETRX_RADIO_CHECK:
        static_cast<UIRadioButton*>(p[0])->Check();
        return 0;
    case EE_SCREENS_SETRX_COMBO_LABEL_TEXT:
        static_cast<UITextLabel*>(static_cast<UIComboBox*>(p[0])->GetTextLabel())->SetText(SetrBytes(p[1], i[0]));
        return 0;
    case EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_ON: return t->mIsOn;
    case EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_DURATION: return (int32_t)t->mConditionDuration;
    case EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_LOOPING: return t->mIsLooping;
    case EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_CINEMATIC: return t->mIsInCinematic;
    case EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_CONDITIONS: return (int32_t)t->GetConditionCount();
    case EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_EFFECTS: return (int32_t)t->GetEffectCount();
    case EE_SCREENS_SETRX_SET + EE_SCREENS_SETR_V_ON:
        t->SetIsOn(i[0] != 0);
        return 0;
    case EE_SCREENS_SETRX_SET + EE_SCREENS_SETR_V_DURATION:
        t->SetConditionDuration((unsigned long)(long)i[0]);
        return 0;
    case EE_SCREENS_SETRX_SET + EE_SCREENS_SETR_V_LOOPING:
        t->SetIsLooping(i[0] != 0);
        return 0;
    case EE_SCREENS_SETRX_SET + EE_SCREENS_SETR_V_CINEMATIC:
        t->SetIsInCinematic(i[0] != 0);
        return 0;
    case EE_SCREENS_SETRX_SET_CONDITION:
        t->SetCondition((unsigned long)i[0], i[1] != 0, (unsigned long)i[2]);
        return 0;
    case EE_SCREENS_SETRX_SET_EFFECT:
        t->SetEffect((unsigned long)i[0], (long)i[1], (unsigned long)i[2]);
        return 0;
    }
    return 0;
}
