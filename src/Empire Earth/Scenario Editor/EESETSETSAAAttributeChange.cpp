// C:\EEX\Empire Earth\Scenario Editor\EESETSETSAAAttributeChange.cpp (file name inferred: no __FILE__; by the
// one-.cpp-per-effect-type rule this range may really be several units: the rest of EESETSETSetAIAttribute,
// EESETSETSetFavoriteUnit, EESETSETSetObjectAttribute, EESETSETSetObjectClassAttribute, EESETSETSetObjectName,
// EESETSETSetObjectOwner). 0x18c72e..0x18e4a4. After EESETSETPlayer.cpp, before EESETSETSetObjectStance.cpp.
// Unit flags: the exe module set; include set copied from EESETSETAI.cpp (inferred).
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
#include "ee/EESETSEffectType.h"
#include "ee/EESETSETAI.h"
#include "ee/EESETSETCAMGModifyObject.h"
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSETObject.h"
#include "ee/EESETSETSAAAttributeChange.h"
#include "ee/EECPBehavior.h"
#include "ee/EEUnit.h"
#include "lle/UIESliderChange.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x58c72e
// Selects the effect's AI variable (else the first), sets its ranges and fills the two int and the float sliders
void EESETSETSetAIAttribute::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    EECPBehavior theBehavior;
    std::vector<long>::iterator theIterator;
    inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    theIterator = std::find(this->mAttributes.begin(), this->mAttributes.end(), (long)inEffect->mAIAttribute);
    if (theIterator != this->mAttributes.end()) {
        inControls->mComboBoxes[1]->SetCurrentSelection(theIterator - this->mAttributes.begin());
        theBehavior = gCPBehaviors[this->mAttributes[theIterator - this->mAttributes.begin()]];
        this->ShowAttributeRanges(theIterator - this->mAttributes.begin(), inControls);
    } else {
        inControls->mComboBoxes[1]->SetCurrentSelection(0);
        theBehavior = gCPBehaviors[this->mAttributes[0]];
        this->ShowAttributeRanges(0, inControls);
    }
    if (inControls->mSliders[0]->GetCurrentRangeValue() != inEffect->mIntValue1)
        inControls->mSliders[0]->SetCurrentRangeValue(inEffect->mIntValue1);
    inControls->mLabels[6]->SetText(inEffect->mIntValue1);
    if (inControls->mSliders[1]->GetCurrentRangeValue() != inEffect->mIntValue2)
        inControls->mSliders[1]->SetCurrentRangeValue(inEffect->mIntValue2);
    inControls->mLabels[9]->SetText(inEffect->mIntValue2);
    if (theBehavior.mFloatSliderScale == 0)
        theBehavior.mFloatSliderScale = 1;
    long theRangeValue = (long)(inEffect->GetFloatValue() * theBehavior.mFloatSliderScale + 0.0001f);
    if (inControls->mSliders[2]->GetCurrentRangeValue() != theRangeValue)
        inControls->mSliders[2]->SetCurrentRangeValue(theRangeValue);
    sprintf(theText, "%g", inEffect->GetFloatValue());
    inControls->mLabels[12]->SetText(theText);
    EESETSETSAAAttributeChange* theEvent = new EESETSETSAAAttributeChange(this, inControls);
    inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
}

// SYNTHETIC: EXE 0x58c941
// (EESETSETSAAAttributeChange's scalar deleting destructor)

// FUNCTION: EXE 0x58c96c
// Shows the AI variable combo (every settable AI behaviour) and the int value 1 / 2 and float value sliders with labels
void EESETSETSetAIAttribute::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation(405.0f, 641.0f);
    UIESliderChange* theEvent;
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_TRIGGER_EDITOR_ATTRIBUTE_7695, theText);
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mComboBoxes[1]->SetLocation(theLocation);
    inControls->mComboBoxes[1]->ChangeWidth(200);
    inControls->mComboBoxes[1]->DeleteAllEntries();
    this->mAttributes.clear();
    for (long i = 0; i < sizeof(gCPBehaviors) / sizeof(EECPBehavior); i++) {
        EECPBehavior theBehavior = gCPBehaviors[i];
        if (theBehavior.mIsSettable != true)
            continue;
        gResource->GetString(i + IDS_CP_BEHAVIOR_FIRST, theText);
        inControls->mComboBoxes[1]->AppendLine(theText);
        this->mAttributes.push_back(i);
    }
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetXY(10.0f, 692.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 125.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_SET_INT_VALUE_1, theText);
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[4]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[5]->SetAlignment(kTextAlignRight);
    theLocation.SetX(10.0f);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 125.0f, theLocation.GetY() + 20.0f));
    inControls->mSliders[0]->SetRectangle(theRect);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeShown);
    theEvent = new UIESliderChange(inControls->mLabels[6], 1.0f, 0);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetX(inControls->mSliders[0]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[6]->SetRectangle(theRect);
    inControls->mLabels[6]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[6]->SetAlignment(kTextAlignRight);
    theLocation.SetXY(220.0f, 692.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 125.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_SET_INT_VALUE_2, theText);
    inControls->mLabels[7]->SetRectangle(theRect);
    inControls->mLabels[7]->SetText(theText);
    theLocation.SetX(inControls->mLabels[7]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[8]->SetRectangle(theRect);
    inControls->mLabels[8]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[8]->SetAlignment(kTextAlignRight);
    theLocation.SetX(220.0f);
    theLocation.SetY(inControls->mLabels[7]->GetRectangle().GetBottom() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 125.0f, theLocation.GetY() + 20.0f));
    inControls->mSliders[1]->SetRectangle(theRect);
    inControls->mSliders[1]->SetDisplayMode(kDisplayModeShown);
    theEvent = new UIESliderChange(inControls->mLabels[9], 1.0f, 0);
    inControls->mSliders[1]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetX(inControls->mSliders[1]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[9]->SetRectangle(theRect);
    inControls->mLabels[9]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[9]->SetAlignment(kTextAlignRight);
    theLocation.SetXY(405.0f, 692.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 125.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_SET_FLOAT_VALUE, theText);
    inControls->mLabels[10]->SetRectangle(theRect);
    inControls->mLabels[10]->SetText(theText);
    inControls->mLabels[10]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[10]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[11]->SetRectangle(theRect);
    inControls->mLabels[11]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[11]->SetAlignment(kTextAlignRight);
    theLocation.SetX(405.0f);
    theLocation.SetY(inControls->mLabels[10]->GetRectangle().GetBottom() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 125.0f, theLocation.GetY() + 20.0f));
    inControls->mSliders[2]->SetRectangle(theRect);
    inControls->mSliders[2]->SetDisplayMode(kDisplayModeShown);
    theEvent = new UIESliderChange(inControls->mLabels[12], 1.0f, 0);
    inControls->mSliders[2]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetX(inControls->mSliders[2]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[12]->SetRectangle(theRect);
    inControls->mLabels[12]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[12]->SetAlignment(kTextAlignRight);
}

// FUNCTION: EXE 0x58d020
// Sets range, start value and help text of each slider of the chosen AI variable; hides sliders whose min equals max
void EESETSETSetAIAttribute::ShowAttributeRanges(unsigned long inSelection, EESETSEffectControls* inControls)
{
    UWideString theText;
    EECPBehavior theBehavior = gCPBehaviors[this->mAttributes[inSelection]];
    if (theBehavior.mIntValue1Min != theBehavior.mIntValue1Max) {
        sprintf(theText, "%d:%d", theBehavior.mIntValue1Min, theBehavior.mIntValue1Max);
        inControls->mLabels[5]->SetText(theText);
        inControls->mSliders[0]->SetRange(theBehavior.mIntValue1Min, theBehavior.mIntValue1Max);
        inControls->mSliders[0]->SetCurrentRangeValue(theBehavior.mIntValue1Min);
        inControls->mLabels[6]->SetText(inControls->mSliders[0]->GetCurrentRangeValue());
        inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[6]->SetDisplayMode(kDisplayModeShown);
        inControls->mSliders[0]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[4]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mLabels[6]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mSliders[0]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mLabels[5]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
    } else {
        inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
        inControls->mLabels[6]->SetDisplayMode(kDisplayModeHidden);
        inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
        inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    }
    if (theBehavior.mIntValue2Min != theBehavior.mIntValue2Max) {
        sprintf(theText, "%d:%d", theBehavior.mIntValue2Min, theBehavior.mIntValue2Max);
        inControls->mLabels[8]->SetText(theText);
        inControls->mSliders[1]->SetRange(theBehavior.mIntValue2Min, theBehavior.mIntValue2Max);
        inControls->mSliders[1]->SetCurrentRangeValue(theBehavior.mIntValue2Min);
        inControls->mLabels[9]->SetText(inControls->mSliders[1]->GetCurrentRangeValue());
        inControls->mLabels[7]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[9]->SetDisplayMode(kDisplayModeShown);
        inControls->mSliders[1]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[8]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[7]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mLabels[9]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mSliders[1]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mLabels[8]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
    } else {
        inControls->mLabels[7]->SetDisplayMode(kDisplayModeHidden);
        inControls->mLabels[9]->SetDisplayMode(kDisplayModeHidden);
        inControls->mSliders[1]->SetDisplayMode(kDisplayModeHidden);
        inControls->mLabels[8]->SetDisplayMode(kDisplayModeHidden);
    }
    if (theBehavior.mFloatSliderMin != theBehavior.mFloatSliderMax) {
        sprintf(theText, "%d:%d", theBehavior.mFloatSliderMin, theBehavior.mFloatSliderMax);
        inControls->mLabels[11]->SetText(theText);
        if (theBehavior.mFloatSliderScale == 0)
            theBehavior.mFloatSliderScale = 1;
        inControls->mSliders[2]->SetRange(theBehavior.mFloatSliderMin, theBehavior.mFloatSliderMax);
        inControls->mSliders[2]->SetCurrentRangeValue(theBehavior.mFloatSliderMin);
        float theScale = (float)theBehavior.mFloatSliderScale;
        float theModifier = 1.0f / theScale;
        static_cast<UIESliderChange*>(inControls->mSliders[2]->GetTriggerEvent(kControlTriggerRangeChanged))->SetModifier(theModifier);
        sprintf(theText, "%g", inControls->mSliders[2]->GetCurrentRangeValue() / theScale);
        inControls->mLabels[12]->SetText(theText);
        inControls->mLabels[10]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[12]->SetDisplayMode(kDisplayModeShown);
        inControls->mSliders[2]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[11]->SetDisplayMode(kDisplayModeShown);
        inControls->mLabels[10]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mLabels[12]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mSliders[2]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
        inControls->mLabels[11]->SetStringTableID(this->mAttributes[inSelection] + IDS_CP_BEHAVIOR_HELP_FIRST);
    } else {
        inControls->mLabels[10]->SetDisplayMode(kDisplayModeHidden);
        inControls->mLabels[12]->SetDisplayMode(kDisplayModeHidden);
        inControls->mSliders[2]->SetDisplayMode(kDisplayModeHidden);
        inControls->mLabels[11]->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x58d435
// Stores the favourite air, land and sea unit types picked in the three combos
void EESETSETSetFavoriteUnit::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mObject = this->mAirUnits[theSelection];
    if (inControls->mComboBoxes[2]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mIntValue1 = this->mLandUnits[theSelection];
    if (inControls->mComboBoxes[3]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mIntValue2 = this->mSeaUnits[theSelection];
}

// FUNCTION: EXE 0x58d492
// Hides the favourite air, land and sea labels and combos
void EESETSETSetFavoriteUnit::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[3]->SetDisplayMode(kDisplayModeHidden);
}

// the find values are rvalue temporaries (the second and third in dead parameter slots)
// FUNCTION: EXE 0x58d4d5
// Selects the effect's favourite air, land and sea unit types (the first entry when one is not listed)
void EESETSETSetFavoriteUnit::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    std::vector<long>::iterator theIterator;
    theIterator = std::find(this->mAirUnits.begin(), this->mAirUnits.end(), (long)inEffect->mObject);
    if (theIterator != this->mAirUnits.end())
        inControls->mComboBoxes[1]->SetCurrentSelection(theIterator - this->mAirUnits.begin());
    else
        inControls->mComboBoxes[1]->SetCurrentSelection(0);
    theIterator = std::find(this->mLandUnits.begin(), this->mLandUnits.end(), inEffect->GetIntValue1());
    if (theIterator != this->mLandUnits.end())
        inControls->mComboBoxes[2]->SetCurrentSelection(theIterator - this->mLandUnits.begin());
    else
        inControls->mComboBoxes[2]->SetCurrentSelection(0);
    theIterator = std::find(this->mSeaUnits.begin(), this->mSeaUnits.end(), inEffect->GetIntValue2());
    if (theIterator != this->mSeaUnits.end())
        inControls->mComboBoxes[3]->SetCurrentSelection(theIterator - this->mSeaUnits.begin());
    else
        inControls->mComboBoxes[3]->SetCurrentSelection(0);
}

// FUNCTION: EXE 0x58d608
// Sorts the scenario-selectable unit types into air, sea and land lists and shows them in three combos
void EESETSETSetFavoriteUnit::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    unsigned long i;
    this->mLandUnits.clear();
    this->mSeaUnits.clear();
    this->mAirUnits.clear();
    for (i = 0; i < EEUnitClass::GetUnitTypes().size(); i++) {
        EEUnitClass* theClass = gUnitClassTable[EEUnitClass::GetUnitTypes()[i]];
        if (theClass->mIsScenarioSelectable != true)
            continue;
        if ((theClass->mDomains & kMovementAir) == kMovementAir)
            this->mAirUnits.push_back(theClass->mUnitType);
        else if ((theClass->mDomains & kMovementSea) == kMovementSea)
            this->mSeaUnits.push_back(theClass->mUnitType);
        else
            this->mLandUnits.push_back(theClass->mUnitType);
    }
    theLocation.SetX(10.0f);
    theLocation.SetY(inControls->mPlayers[0]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    gResource->GetString(IDS_FAVORITE_AIR, theText);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[3]->GetRectangle().GetRight() + 10.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    gResource->GetString(IDS_FAVORITE_LAND, theText);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[4]->GetRectangle().GetRight() + 10.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    gResource->GetString(IDS_FAVORITE_SEA, theText);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[3]->GetRectangle().GetLeft());
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[1]->SetLocation(theLocation);
    inControls->mComboBoxes[1]->ChangeWidth(200);
    inControls->mComboBoxes[1]->DeleteAllEntries();
    for (i = 0; i < this->mAirUnits.size(); i++) {
        gResource->GetString(gUnitClassTable[this->mAirUnits[i]]->mNameStringID, theText);
        inControls->mComboBoxes[1]->AppendLine(theText);
    }
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[4]->GetRectangle().GetLeft());
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[2]->SetLocation(theLocation);
    inControls->mComboBoxes[2]->ChangeWidth(200);
    inControls->mComboBoxes[2]->DeleteAllEntries();
    for (i = 0; i < this->mLandUnits.size(); i++) {
        gResource->GetString(gUnitClassTable[this->mLandUnits[i]]->mNameStringID, theText);
        inControls->mComboBoxes[2]->AppendLine(theText);
    }
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[5]->GetRectangle().GetLeft());
    theLocation.SetY(inControls->mLabels[5]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[3]->SetLocation(theLocation);
    inControls->mComboBoxes[3]->ChangeWidth(200);
    inControls->mComboBoxes[3]->DeleteAllEntries();
    for (unsigned long j = 0; j < this->mSeaUnits.size(); j++) {
        gResource->GetString(gUnitClassTable[this->mSeaUnits[j]]->mNameStringID, theText);
        inControls->mComboBoxes[3]->AppendLine(theText);
    }
    inControls->mComboBoxes[3]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58da54
// Stores the unit attribute, the mode (increase / decrease / exactly), the amount and the percent flag
void EESETSETSetObjectAttribute::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inEffect->mUsesPercent = inControls->mCheckBoxes[0]->GetIsChecked();
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    inEffect->mAttributeMode = theSelection;
    inControls->mEffectComboBoxes[0]->GetCurrentSelection(theSelection);
    inEffect->mAttribute = theSelection;
    inEffect->mAttributeAmount = inControls->mEditBoxes[0]->GetTextAsInt(0);
}

// FUNCTION: EXE 0x58daaa
// Hides the attribute label, attribute and mode combos, percent check box and amount box
void EESETSETSetObjectAttribute::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58dae7
// Selects the effect's attribute and mode without firing their events, fills the amount and sets the percent box
void EESETSETSetObjectAttribute::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mEffectComboBoxes[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mEffectComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mEffectComboBoxes[0]->SetCurrentSelection(inEffect->mAttribute);
    if (theAction) {
        inControls->mEffectComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    theAction = inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mAttributeMode);
    if (theAction) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    inControls->mEditBoxes[0]->SetText(inEffect->mAttributeAmount);
    if (inControls->mCheckBoxes[0]->GetIsChecked() != inEffect->mUsesPercent)
        inControls->mCheckBoxes[0]->SetChecked(inEffect->mUsesPercent);
}

// FUNCTION: EXE 0x58dbb5
// Shows the unit attribute combo (20 attributes), the mode combo, the 'use percent' box and an amount box (0)
void EESETSETSetObjectAttribute::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    GEText* theGEText;
    gResource->GetString(IDS_TRIGGER_EDITOR_ATTRIBUTE_7695, theText);
    theLocation.SetX(430.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(430.0f);
    theLocation.SetY(641.0f);
    inControls->mEffectComboBoxes[0]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[0]->ChangeWidth(200);
    inControls->mEffectComboBoxes[0]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_ATTRIBUTE_FIRST; theIndex < 0x71c0; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[0]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mEffectComboBoxes[0]->GetRectangle().GetBottom() + 10.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(100);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theOperator = IDS_INCREASE; theOperator <= IDS_EXACTLY; theOperator++) {
        gResource->GetString(theOperator, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(430.0f);
    theLocation.SetY(inControls->mEffectComboBoxes[1]->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_USE_PERCENT, theText);
    inControls->mCheckBoxes[0]->SetText(theText, gU531934);
    theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->ChangeWidth(100);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mEffectComboBoxes[1]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mEffectComboBoxes[1]->GetRectangle().GetTop());
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58ddeb
// Stores the class attribute (combo index + 20), increase or decrease, the amount and the percent flag
void EESETSETSetObjectClassAttribute::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inEffect->mUsesPercent = inControls->mCheckBoxes[0]->GetIsChecked();
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    inEffect->mAttributeMode = theSelection;
    inControls->mEffectComboBoxes[0]->GetCurrentSelection(theSelection);
    inEffect->mAttribute = theSelection + 0x14;
    inEffect->mAttributeAmount = inControls->mEditBoxes[0]->GetTextAsInt(0);
}

// FUNCTION: EXE 0x58de44
// Selects the effect's class attribute (else the first) and mode (else increase), fills the amount and percent box
void EESETSETSetObjectClassAttribute::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    UIEvent* theAction = inControls->mEffectComboBoxes[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mEffectComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mEffectComboBoxes[0]->SetCurrentSelection(inEffect->mAttribute - 0x14);
    if (inControls->mEffectComboBoxes[0]->GetCurrentSelection(theSelection) != kErrorOK)
        inControls->mEffectComboBoxes[0]->SetCurrentSelection(0);
    if (theAction) {
        inControls->mEffectComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    if (inEffect->mAttributeMode < 2)
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mAttributeMode);
    else
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(0);
    inControls->mEditBoxes[0]->SetText(inEffect->mAttributeAmount);
    inControls->mCheckBoxes[0]->SetChecked(inEffect->mUsesPercent);
}

// FUNCTION: EXE 0x58def4
// Shows the class attribute combo (shock armor+ ... camera focus+), increase/decrease, percent box and amount box
void EESETSETSetObjectClassAttribute::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    GEText* theGEText;
    gResource->GetString(IDS_TRIGGER_EDITOR_ATTRIBUTE_7695, theText);
    theLocation.SetX(430.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(430.0f);
    theLocation.SetY(641.0f);
    inControls->mEffectComboBoxes[0]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[0]->ChangeWidth(200);
    inControls->mEffectComboBoxes[0]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_ARMOR_ATTRIBUTE_FIRST; theIndex <= IDS_CAMERA_FOCUS_PLUS; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[0]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mEffectComboBoxes[0]->GetRectangle().GetBottom() + 10.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(100);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theOperator = IDS_INCREASE; theOperator <= IDS_DECREASE; theOperator++) {
        gResource->GetString(theOperator, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(430.0f);
    theLocation.SetY(inControls->mEffectComboBoxes[1]->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_USE_PERCENT, theText);
    inControls->mCheckBoxes[0]->SetText(theText, gU531934);
    theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->ChangeWidth(100);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mEffectComboBoxes[1]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mEffectComboBoxes[1]->GetRectangle().GetTop());
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58e12a
// Stores the typed new object name into the effect
void EESETSETSetObjectName::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theName(inControls->mTextEditBox->GetText(0));
    inEffect->mNewName = theName;
}

// FUNCTION: EXE 0x58e168
// Hides the new-name label and edit box
void EESETSETSetObjectName::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58e185
// Shows the effect's new object name in the edit box
void EESETSETSetObjectName::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mTextEditBox->SetText(inEffect->mNewName);
}

// FUNCTION: EXE 0x58e19f
// Shows the 'Name' label and an empty new-name edit box
void EESETSETSetObjectName::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TRIGGER_EDITOR_NAME, theText);
    theLocation.SetX(430.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 150.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(430.0f);
    theLocation.SetY(666.0f);
    inControls->mTextEditBox->SetText("");
    inControls->mTextEditBox->SetLocation(theLocation);
    inControls->mTextEditBox->GetGEText()->SetFont(gU531930);
    inControls->mTextEditBox->ChangeWidth(0x96);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58e2a9
// Stores the new owner picked in the player picker
void EESETSETSetObjectOwner::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mNewOwner = inControls->mPlayers[0]->GetSelectedPlayer();
}

// FUNCTION: EXE 0x58e2c2
// Hides the 'to' label (resetting its alignment), the owner label and the player picker
void EESETSETSetObjectOwner::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58e2f3
// Selects the effect's new owner in the player picker without firing its event
void EESETSETSetObjectOwner::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mPlayers[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mNewOwner);
    if (theAction) {
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x58e34d
// Shows the 'to' and 'owner' labels and the player picker of the owner action
void EESETSETSetObjectOwner::Show(EESETSEffectControls* inControls)
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
    gResource->GetString(IDS_TRIGGER_EDITOR_OWNER, theText);
    theLocation.SetX(inControls->mLabels[3]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 5.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0xe7);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
}

// ICF-FOLDED: EXE 0x58daaa
// Hides the attribute label, attribute and mode combos, percent check box and amount box
// (no address of its own: the same body as EESETSETSetObjectAttribute::VirtualC, which ICF kept)
void EESETSETSetObjectClassAttribute::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}
