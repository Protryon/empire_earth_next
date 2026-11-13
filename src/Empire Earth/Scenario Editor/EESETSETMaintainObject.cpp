// C:\EEX\Empire Earth\Scenario Editor\EESETSETMaintainObject.cpp (file name inferred: no __FILE__). 0x185f75..0x185fed: EESETSETMaintainObject's VirtualC (ICF-shared with EESETSETBuildObject's) and Virtual10.
// Split out of EESETSETGame.cpp: the effect-type classes keep one .cpp each, in
// alphabetical link order (EESETSETBuildObject's slots 0x18357c.. lie between EESETSETAI.cpp and
// EESETSETCAMGModifyObject.cpp; Log < Maintain < Media here). Unit flags: the exe module set; include set copied
// from EESETSETGame.cpp (inferred).
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
#include "ee/EESETSETGame.h"
#include "ee/EESETSETObject.h"
#include "ee/EESETSETAI.h"
#include "ee/EEStringIDs.h"

// FUNCTION: EXE 0x585f75
// Hides the object label and the object combo box (shared by the build and maintain object effects)
void EESETSETBuildObject::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x585f92
// Selects the effect's object in the object combo box without firing its selection-changed event
void EESETSETMaintainObject::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mTargetObject);
    if (theAction) {
        theAction->Dereference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
    }
}

// ICF-FOLDED: EXE 0x5835f1
// Shows the 'Object' label and the object combo filled with the scenario's object names (build and maintain object)
// (no address of its own: the same body as EESETSETBuildObject::Show, which ICF kept)
void EESETSETMaintainObject::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    std::vector<UWideString> theNames;
    theLocation.SetX(405.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[1]->mMode = kSETriggerScreenObject;
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    gWorld.mScenarioManager->GetObjectNames(theNames);
    inControls->mScenarioComboBoxes[1]->AppendStringArray(theNames);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// ICF-FOLDED: EXE 0x58357c
// Stores the object chosen in the object combo as the effect's target (shared by the build and maintain object actions)
// (no address of its own: the same body as EESETSETBuildObject::Store, which ICF kept)
void EESETSETMaintainObject::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mScenarioComboBoxes[1]->GetCurrentSelection(inEffect->mTargetObject);
}
