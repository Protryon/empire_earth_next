// C:\EEX\Empire Earth\Scenario Editor\EESETSETSetObjectStance.cpp (file name inferred: no __FILE__). 0x18e4a4..0x18efb7:
// the effect types EESETSETSetObjectStance (ctor/dtor + four slots), EESETSETToggleAI and EESETSETTrigger (slots), then
// EEScenarioManager::GetTrigger's inline COMDAT 0x18ef8a (first user EESETSETTrigger::UpdateName). By the one-.cpp-per-
// effect-type rule (Set < Toggle < Trigger) this may really be three units; kept as one (inferred). EESETSObject.cpp
// follows at 0x18efb7. Unit flags: the exe module set; include set copied from EESETSETAI.cpp.
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
#include "ee/EESETSETAI.h"
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSETTrigger.h"
#include "ee/EESETSETSetObjectStance.h"
#include "ee/EESMTrigger.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x58e4a4
// Creates the stance action type with its own change-effect event (never attached in this unit)
EESETSETSetObjectStance::EESETSETSetObjectStance()
{
    this->mAction = new EESEEChangeEffectAction();
}

// SYNTHETIC: EXE 0x58e4d5
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETSetObjectStance+0x0)

// FUNCTION: EXE 0x58e4f1
// Destroys the stance action type, releasing its change-effect event
EESETSETSetObjectStance::~EESETSETSetObjectStance()
{
    this->mAction->Dereference();
}

// FUNCTION: EXE 0x58e50b
// Stores the stance picked in the stance combo
void EESETSETSetObjectStance::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(inEffect->mStance);
}

// FUNCTION: EXE 0x58e525
// Hides the 'to' and stance labels and the stance combo
void EESETSETSetObjectStance::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58e54c
// Selects the effect's stance in the stance combo without firing its event
void EESETSETSetObjectStance::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theEvent = inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent) {
        theEvent->Reference();
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mStance);
    if (theEvent) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
}

// FUNCTION: EXE 0x58e5a7
// Shows the 'to' and 'Stance' labels and the stance combo (explore ... aggressive)
void EESETSETSetObjectStance::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TO, theText);
    theLocation.SetX(425.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 25.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_EDITOR_STANCE, theText);
    theLocation.SetX(inControls->mLabels[3]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 5.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(200);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_UNIT_BEHAVIOR_FIRST; theIndex <= IDS_AGGRESSIVE; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58e72b
// Stores on or off and the AI behaviour to toggle
void EESETSETToggleAI::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mIsOn = inControls->mRadioButtons[0]->GetIsChecked();
    inControls->mComboBoxes[1]->GetCurrentSelection(inEffect->mAIToggle);
}

// FUNCTION: EXE 0x58e757
// Hides the AI toggle label and combo and the on/off radios
void EESETSETToggleAI::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58e78e
// Checks on or off and selects the effect's AI toggle without firing the combo event
void EESETSETToggleAI::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    if (inEffect->mIsOn == true)
        inControls->mRadioButtons[0]->Check();
    else
        inControls->mRadioButtons[1]->Check();
    UIEvent* theEvent = inControls->mComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent) {
        theEvent->Reference();
        inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mComboBoxes[1]->SetCurrentSelection(inEffect->mAIToggle);
    if (theEvent) {
        inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
}

// FUNCTION: EXE 0x58e80c
// Shows the AI toggle combo (active ... gather) and the on/off radios
// The names vector is unused (a dead local: its _Buy(0) and _Tidy survive).
void EESETSETToggleAI::Show(EESETSEffectControls* inControls)
{
    std::vector<UWideString> theNames;
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    theLocation.SetX(405.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_AI_TOGGLE, theText);
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[1]->SetLocation(theLocation);
    inControls->mComboBoxes[1]->ChangeWidth(200);
    inControls->mComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_ACTIVE; theIndex <= IDS_TRIGGER_GATHER; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[1]->AppendLine(theText);
    }
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(692.0f);
    gResource->GetString(IDS_GAME_ON, theText);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    GEText* theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[0]->SetLocation(theLocation);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mRadioButtons[0]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_GAME_OFF, theText);
    inControls->mRadioButtons[1]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[1]->SetLocation(theLocation);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58ea18
// Stores the trigger effect, renames it and copies the trigger screen's name box into the effect name
void EESETSETTrigger::ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->Store(inEffect, inControls);
    this->UpdateName(inEffect, inControls);
    inEffect->mName = UWideString(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName());
}

// FUNCTION: EXE 0x58ea75
// Stores the target trigger and the state to switch it to
void EESETSETTrigger::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mScenarioComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mTrigger = theSelection;
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(inEffect->mTriggerState);
}

// FUNCTION: EXE 0x58eaa3
// Hides the trigger effect's title, trigger and state labels and the trigger and state combos
void EESETSETTrigger::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58eadc
// Selects the effect's trigger and state without firing the combos' events
void EESETSETTrigger::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theEvent = inControls->mScenarioComboBoxes[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent)
        theEvent->Reference();
    inControls->mScenarioComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mScenarioComboBoxes[0]->SetCurrentSelection(inEffect->mTrigger);
    if (theEvent) {
        inControls->mScenarioComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
    theEvent = inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent)
        theEvent->Reference();
    inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mTriggerState);
    if (theEvent) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
}

// FUNCTION: EXE 0x58eb78
// Shows the trigger combo (every scenario trigger, with a new change-effect event that is never released)
// and the state combo (on, off, fired, not fired)
void EESETSETTrigger::Show(EESETSEffectControls* inControls)
{
    std::vector<UWideString> theNames;
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TRIGGER_EFFECT, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 206.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_SCREEN_TAB_TRIGGER, theText);
    theLocation.SetX(10.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(10.0f);
    theLocation.SetY(666.0f);
    inControls->mScenarioComboBoxes[0]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[0]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[0]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[0]->mMode = kSETriggerScreenTrigger;
    gWorld.mScenarioManager->GetTriggerNames(theNames);
    for (unsigned long i = 0; i < theNames.size(); i++)
        inControls->mScenarioComboBoxes[0]->AppendLine(theNames[i]);
    inControls->mScenarioComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
    // SetTriggerEvent takes its own reference: drop the creation reference (the event leaked).
    EESEEChangeEffectAction* theChangeEvent = new EESEEChangeEffectAction();
    inControls->mScenarioComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theChangeEvent);
    theChangeEvent->Dereference();
    gResource->GetString(IDS_STATE, theText);
    theLocation.SetX(inControls->mLabels[0]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[1]->SetRectangle(theRect);
    inControls->mLabels[1]->SetText(theText);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(200);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_SWITCH_STATE_FIRST; theIndex <= IDS_NOT_FIRED; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58ee17
// Renames a default-named <...> effect to <trigger name state> and shows it in the name box
void EESETSETTrigger::UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    UWideString theName;
    UWideString theType;
    theText = inEffect->mName;
    if (theText.GetLength() > 0 && theText[0] == '<') {
        unsigned long theIndex = inEffect->mTrigger;
        EEScenarioManager* theManager = gWorld.mScenarioManager;
        EESMTrigger* theTrigger = theManager->GetTrigger(theIndex);
        if (theTrigger)
            theName = theTrigger->mName;
        else
            gResource->GetString(IDS_NONE, theName);
        unsigned long theSelection;
        inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
        gResource->GetString(theSelection + IDS_TRIGGER_SWITCH_STATE_FIRST, theType);
        theText = "<" + theName + " " + theType + ">";
        inEffect->mName = theText;
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mNameEditBox->SetText(theText);
    }
}

// FUNCTION: EXE 0x58ef8a
// Returns the trigger at an index, or NULL (EEScenarioManager::GetTrigger, inline; emitted after its first user)
