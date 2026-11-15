// C:\EEX\Empire Earth\Scenario Editor\EESETSTrigger.cpp (file name inferred: no __FILE__; RTTI class EESETSTrigger).
// The trigger screen's trigger sub-screen: 0x19393b..0x196ebe (ctor, its combo box class's inline ctor and
// MouseRButtonUp, the ??_G pair, the dtor, the slots and members in vtable order of their bodies, the trigger's
// GetConditionCount COMDAT, then the trigger events' ??_G). After EESETSObject.cpp (whose range ends with
// vector<UWideString>::operator= 0x1937b1), before EESETSUICBPlayer.cpp (0x196ebe). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include <vector>
#include <deque>
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESMArea.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESMCondition.h"
#include "ee/EESMTrigger.h"
#include "ee/EESETSTriggerUIComboBox.h"
#include "ee/EESETSTrigger.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "ee/EESMTConditionWrapper.h"
#include "ee/EESMTEffectWrapper.h"

// NEAR: register choice (the original keeps this in edi and the selection in a stack slot) and the screen via the refused vector operator[]
// Opens the tab of the entry picked in the combo box on a right click and selects it there: triggers,
// conditions (object conditions go to the object tab), effects, objects or areas
long EESETSTriggerUIComboBox::MouseRButtonUp()
{
    EESETriggerScreen* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen();
    unsigned long theSelection;
    unsigned long theIndex;
    UIEvent* theEvent;
    switch (this->mKind) {
    case kSETriggerScreenTrigger:
        theSelection = this->GetSelectionIndex();
        if (!UIsNone32(theSelection)) {
            theEvent = theScreen->GetTab(kSETriggerScreenTrigger)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenTrigger)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenTrigger)->Check();
            theScreen->GetTab(kSETriggerScreenTrigger)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenTrigger]->mSelection = theSelection;
            theScreen->ShowTab(kSETriggerTabTrigger);
        }
        break;
    case kSETriggerScreenCondition:
        theSelection = this->GetSelectionIndex();
        if (!UIsNone32(theSelection)) {
            unsigned long theCount = gWorld.mScenarioManager->mConditions.size();
            for (theIndex = 0; theIndex < theCount; theIndex++) {
                if (gWorld.mScenarioManager->GetCondition(theIndex)->GetType() != kSMConditionObject)
                    continue;
                if (theIndex != 0 && theSelection >= theIndex) {
                    theEvent = theScreen->GetTab(kSETriggerScreenObject)->GetTriggerEvent(kControlTriggerChecked);
                    if (theEvent != NULL)
                        theEvent->Reference();
                    theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, NULL);
                    theScreen->GetTab(kSETriggerScreenObject)->Check();
                    theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, theEvent);
                    theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - theIndex;
                    theScreen->ShowTab(kSETriggerTabObject);
                    return 0;
                }
                break;
            }
            if (theSelection > 0) {
                theEvent = theScreen->GetTab(kSETriggerScreenCondition)->GetTriggerEvent(kControlTriggerChecked);
                if (theEvent != NULL)
                    theEvent->Reference();
                theScreen->GetTab(kSETriggerScreenCondition)->SetTriggerEvent(kControlTriggerChecked, NULL);
                theScreen->GetTab(kSETriggerScreenCondition)->Check();
                theScreen->GetTab(kSETriggerScreenCondition)->SetTriggerEvent(kControlTriggerChecked, theEvent);
                theScreen->mScreens[kSETriggerScreenCondition]->mSelection = theSelection - 1;
                theScreen->ShowTab(kSETriggerTabCondition);
            }
        }
        break;
    case kSETriggerScreenEffect:
        theSelection = this->GetSelectionIndex();
        if (!UIsNone32(theSelection) && theSelection > 0) {
            theEvent = theScreen->GetTab(kSETriggerScreenEffect)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenEffect)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenEffect)->Check();
            theScreen->GetTab(kSETriggerScreenEffect)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenEffect]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabEffect);
        }
        break;
    case kSETriggerScreenObject:
        theSelection = this->GetSelectionIndex();
        if (!UIsNone32(theSelection) && theSelection > 0) {
            theEvent = theScreen->GetTab(kSETriggerScreenObject)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenObject)->Check();
            theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabObject);
        }
        break;
    case kSETriggerScreenArea:
        theSelection = this->GetSelectionIndex();
        if (!UIsNone32(theSelection) && theSelection > 1) {
            theEvent = theScreen->GetTab(kSETriggerScreenArea)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenArea)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenArea)->Check();
            theScreen->GetTab(kSETriggerScreenArea)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenArea]->mSelection = theSelection - 2;
            theScreen->ShowTab(kSETriggerTabArea);
        }
        break;
    }
    return 0;
}

// The trigger screen whole in Rust (ee_screens editor_trigger.rs): the constructor (0x59393b), the destructor
// (0x595fde), Sort, MoveDown, MoveUp, Delete, UpdateSecondConditionRow, UpdateSecondEffectRow, Duplicate, Activate,
// Store (0x596415), ChangeOperator, GetNames, Hide, Load (0x596785), New, RemoveEntry, ShowOperator, Show and the
// rows' steps are its members; the class forwards. EESETSTriggerUIComboBox stays C++ (above), and this unit is the
// screen's host (ee_screenscpp_setr).

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// Builds the trigger sub-screen: if/then labels, two condition and two effect rows (not, combo, and/or,
// delay, remove), scroll buttons and the is-on, condition timer, looping and cinematic check boxes (editor_trigger.rs)
EESETSTrigger::EESETSTrigger(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_setr_new(inForm, &theIndex);
    ioIndex = theIndex;
}

// Destroys the trigger sub-screen's controls and releases its operator event (editor_trigger.rs)
EESETSTrigger::~EESETSTrigger()
{
    ee_screens_setr_free(this->mRust);
}

static void EESETSTriggerCall(EESETSTrigger* inScreen, uint32_t inWhat, uint32_t inA = 0)
{
    ee_screens_setr_call(inScreen->mRust, inWhat, (uint32_t)inScreen->mSelection, inA);
}

void EESETSTrigger::Sort() { EESETSTriggerCall(this, EE_SCREENS_SETR_LIST_OP, EE_SCREENS_SEOB_SM_SORT); }
void EESETSTrigger::MoveDown() { EESETSTriggerCall(this, EE_SCREENS_SETR_LIST_OP, EE_SCREENS_SEOB_SM_DOWN); }
void EESETSTrigger::MoveUp() { EESETSTriggerCall(this, EE_SCREENS_SETR_LIST_OP, EE_SCREENS_SEOB_SM_UP); }
void EESETSTrigger::Delete() { EESETSTriggerCall(this, EE_SCREENS_SETR_LIST_OP, EE_SCREENS_SEOB_SM_DELETE); }
void EESETSTrigger::Duplicate() { EESETSTriggerCall(this, EE_SCREENS_SETR_LIST_OP, EE_SCREENS_SEOB_SM_DUPLICATE); }
void EESETSTrigger::New() { EESETSTriggerCall(this, EE_SCREENS_SETR_LIST_OP, EE_SCREENS_SEOB_SM_NEW); }
void EESETSTrigger::Activate() { EESETSTriggerCall(this, EE_SCREENS_SETR_ACTIVATE); }
void EESETSTrigger::Store() { EESETSTriggerCall(this, EE_SCREENS_SETR_STORE); }
void EESETSTrigger::Load() { EESETSTriggerCall(this, EE_SCREENS_SETR_LOAD); }
void EESETSTrigger::Hide() { EESETSTriggerCall(this, EE_SCREENS_SETR_HIDE); }
void EESETSTrigger::Show() { EESETSTriggerCall(this, EE_SCREENS_SETR_SHOW); }
void EESETSTrigger::ChangeOperator(unsigned long inControlID) { EESETSTriggerCall(this, EE_SCREENS_SETR_CHANGE_OPERATOR, (uint32_t)inControlID); }
void EESETSTrigger::RemoveEntry(unsigned long inControlID) { EESETSTriggerCall(this, EE_SCREENS_SETR_REMOVE_ENTRY, (uint32_t)inControlID); }
void EESETSTrigger::DecrementA4() { EESETSTriggerCall(this, EE_SCREENS_SETR_STEP, 0); }
void EESETSTrigger::IncrementA4() { EESETSTriggerCall(this, EE_SCREENS_SETR_STEP, 1); }
void EESETSTrigger::DecrementA8() { EESETSTriggerCall(this, EE_SCREENS_SETR_STEP, 2); }
void EESETSTrigger::IncrementA8() { EESETSTriggerCall(this, EE_SCREENS_SETR_STEP, 3); }
void EESETSTrigger::GetNames(std::vector<UWideString>& outNames) { ee_screens_setr_get_names(this->mRust, &outNames); }

void EESETSTrigger::Deactivate()
{
}

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
    auto theScreen = [] { return gUIManager->mScenarioEditorForm->GetTriggerScreen(); };
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
        theScreen()->HideText();
        theScreen()->HideAlphaSort();
        return 0;
    case EE_SCREENS_SETR_OP_SHOW_TEXT: theScreen()->ShowText(); return 0;
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
        t->SetName(UWideString(theScreen()->GetName()));
        theScreen()->GetTextEditBox()->GetAllText(theText);
        t->SetText(theText);
        return 0;
    }
    case EE_SCREENS_SETR_OP_TEXTS_TO_SCREEN:
        theScreen()->GetNameEditBox()->SetText(t->mName);
        theScreen()->GetTextEditBox()->SetText(t->mText);
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
    case EE_SCREENS_SETR_OP_SET_EVENT:
        static_cast<UIControl*>(p[0])->SetTriggerEvent(kControlTriggerSelectionChanged, static_cast<UIEvent*>(p[1]));
        return 0;
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
