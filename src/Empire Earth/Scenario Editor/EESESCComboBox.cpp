// C:\EEX\Empire Earth\Scenario Editor\EESESCComboBox.cpp (file name inferred: no __FILE__). 0x17acf6..0x17b0b7: the unit's
// global UString's `$E` chain, then EESETSEScenarioComboBox's ChangeSelection and MouseRButtonUp slots. After
// EESERBUnitTypes.cpp (0x173218), before EESETriggerScreen.cpp (0x17b0b7). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
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
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"

#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EEMedia.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESMCondition.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EEScenarioIDs.h"

// the unit's global's `$E` chain (a plain global: unguarded atexit dtor) (provisional name)
UString gSECivilizationExtension(".civ");

// Changes the selection and refreshes the controls of the trigger screen the combo sits on
void EESETSEScenarioComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    switch (this->mScreen) {
    case kSETriggerScreenTrigger:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenTrigger]->RefreshControls();
        break;
    case kSETriggerScreenCondition:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenCondition]->RefreshControls();
        break;
    case kSETriggerScreenEffect:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenEffect]->RefreshControls();
        break;
    case kSETriggerScreenObject:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenObject]->RefreshControls();
        break;
    case kSETriggerScreenArea:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenArea]->RefreshControls();
        break;
    }
}

// Jumps to the trigger, condition, effect, object or area entry right-clicked in the combo (skipping the
// list's leading built-in entries)
long EESETSEScenarioComboBox::MouseRButtonUp()
{
    EESETriggerScreen* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen();
    unsigned long theSelection;
    switch (this->mMode) {
    case kSETriggerScreenTrigger:
        this->GetCurrentSelection(theSelection);
        if (!UIsNone32(theSelection)) {
            UIEvent* theEvent = theScreen->GetTab(kSETriggerScreenTrigger)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenTrigger)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenTrigger)->Check();
            theScreen->GetTab(kSETriggerScreenTrigger)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenTrigger]->mSelection = theSelection;
            theScreen->ShowTab(kSETriggerTabTrigger);
        }
        break;
    case kSETriggerScreenCondition:
        this->GetCurrentSelection(theSelection);
        if (!UIsNone32(theSelection)) {
            unsigned long theConditionCount = gWorld.mScenarioManager->mConditions.size();
            unsigned long theObjectCondition = 0;
            while (theObjectCondition < theConditionCount) {
                if (gWorld.mScenarioManager->GetCondition(theObjectCondition)->mType == kSMConditionObject)
                    break;
                theObjectCondition++;
            }
            if (theObjectCondition != theConditionCount && theObjectCondition != 0 && theSelection >= theObjectCondition) {
                UIEvent* theEvent = theScreen->GetTab(kSETriggerScreenObject)->GetTriggerEvent(kControlTriggerChecked);
                if (theEvent)
                    theEvent->Reference();
                theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, NULL);
                theScreen->GetTab(kSETriggerScreenObject)->Check();
                theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, theEvent);
                theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - theObjectCondition;
                theScreen->ShowTab(kSETriggerTabObject);
            } else if (theSelection > 0) {
                // Each jump checks its own tab's radio (the condition and object radios were swapped)
                UIEvent* theEvent = theScreen->GetTab(kSETriggerScreenCondition)->GetTriggerEvent(kControlTriggerChecked);
                if (theEvent)
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
        this->GetCurrentSelection(theSelection);
        if (!UIsNone32(theSelection) && theSelection > 0) {
            UIEvent* theEvent = theScreen->GetTab(kSETriggerScreenEffect)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenEffect)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenEffect)->Check();
            theScreen->GetTab(kSETriggerScreenEffect)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenEffect]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabEffect);
        }
        break;
    case kSETriggerScreenObject:
        this->GetCurrentSelection(theSelection);
        if (!UIsNone32(theSelection) && theSelection > 0) {
            UIEvent* theEvent = theScreen->GetTab(kSETriggerScreenObject)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenObject)->Check();
            theScreen->GetTab(kSETriggerScreenObject)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabObject);
        }
        break;
    case kSETriggerScreenArea:
        this->GetCurrentSelection(theSelection);
        if (!UIsNone32(theSelection) && theSelection > 1) {
            UIEvent* theEvent = theScreen->GetTab(kSETriggerScreenArea)->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->GetTab(kSETriggerScreenArea)->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->GetTab(kSETriggerScreenArea)->Check();
            theScreen->GetTab(kSETriggerScreenArea)->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenArea]->mSelection = theSelection - 2;
            theScreen->ShowTab(kSETriggerTabArea);
        }
        break;
    }
    return kErrorOK;
}
