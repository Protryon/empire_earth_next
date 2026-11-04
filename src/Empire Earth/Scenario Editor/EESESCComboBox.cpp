// C:\EEX\Empire Earth\Scenario Editor\EESESCComboBox.cpp (file name inferred: no __FILE__). 0x17acf6..0x17b0b7: the unit's
// global UString's `$E` chain, then EESETSEScenarioComboBox's ChangeSelection and MouseRButtonUp slots. After
// EESERBUnitTypes.cpp (0x173218), before EESETriggerScreen.cpp (0x17b0b7). Unit flags: the exe module set.
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
#include "lle/UITriStateCheckBox.h"
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
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
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
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"
#include "ee/EESMCondition.h"
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "LostDebug.h"
#include "ee/EEScenarioIDs.h"

// SYNTHETIC: EXE 0x57acf6
// SYNTHETIC: EXE 0x57ad00
// SYNTHETIC: EXE 0x57ad11
// SYNTHETIC: EXE 0x57ad1d
// the unit's global's `$E` chain (a plain global: unguarded atexit dtor) (provisional name)
UString gSECivilizationExtension(".civ");   // 0x531fd8

// FUNCTION: EXE 0x57ad28
// Changes the selection and refreshes the controls of the trigger screen the combo sits on; a condition
// screen combo refreshes the effect screen instead
void EESETSEScenarioComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    switch (this->mScreen) {
    case kSETriggerScreenTrigger:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenTrigger]->RefreshControls();
        break;
    case kSETriggerScreenCondition:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mScreens[kSETriggerScreenEffect]->RefreshControls();
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
    EE_LOST_DEBUG(5);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x57add4
// Jumps to the trigger, condition, effect, object or area entry right-clicked in the combo (skipping the
// list's leading built-in entries)
long EESETSEScenarioComboBox::MouseRButtonUp()
{
    EESETriggerScreen* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen();
    unsigned long theSelection;
    switch (this->mMode) {
    case kSETriggerScreenTrigger:
        this->GetCurrentSelection(theSelection);
        if (theSelection != -1) {
            UIEvent* theEvent = theScreen->mTabs[kSETriggerScreenTrigger]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenTrigger]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenTrigger]->Check();
            theScreen->mTabs[kSETriggerScreenTrigger]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenTrigger]->mSelection = theSelection;
            theScreen->ShowTab(kSETriggerTabTrigger);
        }
        break;
    case kSETriggerScreenCondition:
        this->GetCurrentSelection(theSelection);
        if (theSelection != -1) {
            unsigned long theConditionCount = gWorld.mScenarioManager->mConditions.size();
            unsigned long theObjectCondition = 0;
            while (theObjectCondition < theConditionCount) {
                if (gWorld.mScenarioManager->GetCondition(theObjectCondition)->mType == kSMConditionObject)
                    break;
                theObjectCondition++;
            }
            if (theObjectCondition != theConditionCount && theObjectCondition != 0 && theSelection >= theObjectCondition) {
                UIEvent* theEvent = theScreen->mTabs[kSETriggerScreenObject]->GetTriggerEvent(kControlTriggerChecked);
                if (theEvent)
                    theEvent->Reference();
                theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, NULL);
                theScreen->mTabs[kSETriggerScreenObject]->Check();
                theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, theEvent);
                theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - theObjectCondition;
                theScreen->ShowTab(kSETriggerTabObject);
            } else if (theSelection > 0) {
                // Each jump checks its own tab's radio (the condition and object radios were swapped)
                UIEvent* theEvent = theScreen->mTabs[kSETriggerScreenCondition]->GetTriggerEvent(kControlTriggerChecked);
                if (theEvent)
                    theEvent->Reference();
                theScreen->mTabs[kSETriggerScreenCondition]->SetTriggerEvent(kControlTriggerChecked, NULL);
                theScreen->mTabs[kSETriggerScreenCondition]->Check();
                theScreen->mTabs[kSETriggerScreenCondition]->SetTriggerEvent(kControlTriggerChecked, theEvent);
                theScreen->mScreens[kSETriggerScreenCondition]->mSelection = theSelection - 1;
                theScreen->ShowTab(kSETriggerTabCondition);
            }
        }
        break;
    case kSETriggerScreenEffect:
        this->GetCurrentSelection(theSelection);
        if (theSelection != -1 && theSelection > 0) {
            UIEvent* theEvent = theScreen->mTabs[kSETriggerScreenEffect]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenEffect]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenEffect]->Check();
            theScreen->mTabs[kSETriggerScreenEffect]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenEffect]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabEffect);
        }
        break;
    case kSETriggerScreenObject:
        this->GetCurrentSelection(theSelection);
        if (theSelection != -1 && theSelection > 0) {
            UIEvent* theEvent = theScreen->mTabs[kSETriggerScreenObject]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenObject]->Check();
            theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabObject);
        }
        break;
    case kSETriggerScreenArea:
        this->GetCurrentSelection(theSelection);
        if (theSelection != -1 && theSelection > 1) {
            UIEvent* theEvent = theScreen->mTabs[kSETriggerScreenArea]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenArea]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenArea]->Check();
            theScreen->mTabs[kSETriggerScreenArea]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenArea]->mSelection = theSelection - 2;
            theScreen->ShowTab(kSETriggerTabArea);
        }
        break;
    }
    return kErrorOK;
}
