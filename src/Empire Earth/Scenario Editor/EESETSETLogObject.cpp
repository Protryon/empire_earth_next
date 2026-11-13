// C:\EEX\Empire Earth\Scenario Editor\EESETSETLogObject.cpp (file name inferred: no __FILE__). 0x185d31..0x185f75: EESETSETLogObject's four slots.
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
// FUNCTION: EXE 0x585d31
// Stores the effect's degrees value from the edit box and its snap flag from the check box
void EESETSETLogObject::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mAttributeAmount = inControls->mEditBoxes[0]->GetTextAsInt(0);
    inEffect->mUsesPercent = inControls->mCheckBoxes[0]->GetIsChecked();
}

// FUNCTION: EXE 0x585d69
// Hides the degrees and snap labels, the snap check box and the degrees edit box
void EESETSETLogObject::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x585d9d
// Fills the degrees edit box and the snap check box from the effect
void EESETSETLogObject::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mEditBoxes[0]->SetText(inEffect->mAttributeAmount);
    if (inControls->mCheckBoxes[0]->GetIsChecked() != inEffect->mUsesPercent)
        inControls->mCheckBoxes[0]->SetChecked(inEffect->mUsesPercent);
}

// FUNCTION: EXE 0x585dd8
// Shows the 'degrees' label and edit box (reset to 0) and the 'snap' label and check box beside the object combo
void EESETSETLogObject::Show(EESETSEffectControls* inControls)
{
    UWideString theText;
    GE2DPoint theLocation;
    GERectangle theRect;
    gResource->GetString(IDS_DEGREES, theText);
    theLocation.SetX(inControls->mScenarioComboBoxes[1]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 100.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mScenarioComboBoxes[1]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mScenarioComboBoxes[1]->GetRectangle().GetTop());
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SNAP, theText);
    theLocation.SetX(inControls->mEditBoxes[0]->GetRectangle().GetRight() + 50.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 100.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mEditBoxes[0]->GetRectangle().GetRight() + 50.0f);
    theLocation.SetY(inControls->mScenarioComboBoxes[1]->GetRectangle().GetTop());
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->ChangeWidth(0x14);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
}
