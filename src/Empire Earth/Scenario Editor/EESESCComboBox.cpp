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
#include "ee/EEARust.h"
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
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->mScreens[kSETriggerScreenTrigger]->RefreshControls();
        break;
    case kSETriggerScreenCondition:
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->mScreens[kSETriggerScreenCondition]->RefreshControls();
        break;
    case kSETriggerScreenEffect:
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->mScreens[kSETriggerScreenEffect]->RefreshControls();
        break;
    case kSETriggerScreenObject:
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->mScreens[kSETriggerScreenObject]->RefreshControls();
        break;
    case kSETriggerScreenArea:
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->mScreens[kSETriggerScreenArea]->RefreshControls();
        break;
    }
}

// Jumps to the trigger, condition, effect, object or area entry right-clicked in the combo (EESETSTrigger.cpp)
long EESETSEScenarioComboBox::MouseRButtonUp()
{
    EESETriggerJump(this->mMode, this->GetSelectionIndex());
    return kErrorOK;
}
