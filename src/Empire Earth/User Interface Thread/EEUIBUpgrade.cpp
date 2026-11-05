// C:\EEX\Empire Earth\User Interface Thread\EEUIBUpgrade.cpp (file name inferred: no __FILE__; RTTI class EEUIBUpgrade).
// The in-game improvement button: 0x256845..0x257223 (ctor, the in-class mouse forwarders, ??_G, then the out-of-line
// members in alphabetical order; EEUICChatLine's ctor at 0x257223). Unit flags: the exe module set.
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAction.h"
#include "ee/EEAUpgradeImprovement.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEStringIDs.h"
#include "ee/EEImprovementIDs.h"
static const float kBgOpacity = 0.15f;      // 0x449d4c

// FUNCTION: EXE 0x656845
// Creates an improvement button, faintly tinted with a thin border, without a cost label
EEUIBUpgrade::EEUIBUpgrade(unsigned long inId, const GE2DPoint& inLocation, unsigned long inWidth,
                           unsigned long inHeight, UIForm* inForm, unsigned long inIndex, bool inScenarioEditor,
                           unsigned long inBorderThickness)
    : UIButton(inId, NULL, NULL, NULL, GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
               inBorderThickness),
      mScenarioEditor(inScenarioEditor)
{
    this->mNumQueued = 0;
    this->mLevel = 0;
    this->mLastMessageTime = 0;
    this->mCostLabel = NULL;
    this->SetBgOpacity(kBgOpacity);
    this->SetBorderOpacity(kBgOpacity * 3.0f);
    this->SetBorderThickness(1);
    this->mDisabled = false;
}

// FUNCTION: EXE 0x656943
// Handles a left press as a button press
// (EEUIBUpgrade::MouseLButtonDown, in the class)

// FUNCTION: EXE 0x656949
// Handles a left release as a button release
// (EEUIBUpgrade::MouseLButtonUp, in the class)

// SYNTHETIC: EXE 0x65694f
// (the scalar deleting destructor, emitted with the vtable; slot EEUIBUpgrade+0x0)

// FUNCTION: EXE 0x656980
// Presses the button unless the improvement is unavailable, and updates the help text
void EEUIBUpgrade::ButtonDown()
{
    if (!this->GetDisabled())
        UIButton::ButtonDown();
    this->GetForm()->UpdateHelpString();
}

// FUNCTION: EXE 0x6569a2
// Buys the improvement for the selected unit type when possible; otherwise plays the refusal sound and
// shows the reason in the chat log (at most once per world-table delay)
void EEUIBUpgrade::ButtonUp()
{
    UWideString theReason;
    EEUnit* theUnit = NULL;
    if (this->GetCanUpgrade(theUnit, &theReason)) {
        if (theUnit != NULL) {
            gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
            (new EEAUpgradeImprovement(theUnit, this->mAttribute, true, !this->mScenarioEditor))->Dereference();
        }
    } else if (!this->GetDisabled()) {
        gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        if (theReason.GetLength() > 0) {
            unsigned long theDelay = gWorldTable.GetLongValue(kWorldRefusalMessageInterval);
            if (TMGetTime() - this->mLastMessageTime > theDelay) {
                gUserInterface->AddChatLogEntry(theReason, 0);
                this->mLastMessageTime = TMGetTime();
            }
        }
    }
    this->GetForm()->UpdateHelpString();
    if (this->mState == kButtonStatePressed)
        UIButton::ButtonUp();
}

// FUNCTION: EXE 0x656ae2
// Snapshots the button and its cost label for drawing
long EEUIBUpgrade::CopyData()
{
    long theResult = UIButton::CopyData();
    if (theResult == kErrorOK && this->mCostLabel != NULL) {
        this->GetForm()->GetManager()->AddDirtyRectangle(this->mCostLabel->GetRectangle(), this->GetForm());
        theResult = this->mCostLabel->CopyData();
    }
    return theResult;
}

// FUNCTION: EXE 0x656b1e
// Draws the button text and its cost label
long EEUIBUpgrade::DrawTextA()
{
    long theResult = UIButton::DrawTextA();
    if (theResult == kErrorOK && this->mCostLabel != NULL)
        theResult = this->mCostLabel->DrawTextA();
    return theResult;
}

// FUNCTION: EXE 0x656b3c
// Tells whether the improvement can be bought for the selected unit (or, in the scenario editor, upgraded)
bool EEUIBUpgrade::GetCanUpgrade(EEUnit*& outUnit, UWideString* outReason)
{
    bool theResult = false;
    if (!this->mScenarioEditor) {
        EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
        if (theGroup != NULL) {
            EEComplexUnit* theUnit = theGroup->GetFirstUnit();
            if (theUnit != NULL && gUnitImprovementsAllowed == true &&
                theUnit->mPlayer->CanBuy(theUnit->mUnitClass->GetUnitType(), this->mAttribute, outReason)) {
                theResult = true;
                outUnit = theUnit;
            }
        }
    } else {
        EEUnit* theUnit = static_cast<EEUIFScenarioEditor*>(this->GetForm())->GetFirstSelectedUnit();
        outUnit = theUnit;
        if (theUnit != NULL && theUnit->mStats->mImprovement.CanUpgrade(theUnit->mStats->mUnitClass, this->mAttribute) == true)
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x656bd3
// Tells whether the button is unavailable: no unit, improvements off, maxed, or not improvable for the class
bool EEUIBUpgrade::GetDisabled()
{
    bool theResult = true;
    if (!this->mScenarioEditor) {
        EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
        if (theGroup != NULL) {
            EEComplexUnit* theUnit = theGroup->GetFirstUnit();
            if (theUnit != NULL && gUnitImprovementsAllowed == true &&
                !theUnit->mPlayer->IsImprovementMaxed(theUnit->mUnitClass->GetUnitType(), this->mAttribute) &&
                (this->mAttribute != kImprovementRepair ? theUnit->mUnitClass->mImprovements->mEntries[this->mAttribute].mStepFactor : 0.0f) > 0.0f)
                theResult = false;
        }
    } else {
        EEUnit* theUnit = static_cast<EEUIFScenarioEditor*>(this->GetForm())->GetFirstSelectedUnit();
        if (theUnit != NULL && theUnit->mStats->mImprovement.CanUpgrade(theUnit->mStats->mUnitClass, this->mAttribute) == true)
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x656c83
// Builds the help text: improvement name, its next gain and cost, or "at maximum" / "not allowed"
bool EEUIBUpgrade::GetHelpString(UWideString& outString) const
{
    UWideString theText;
    bool theResult = true;
    long theLevel;
    if (!this->mScenarioEditor) {
        if (gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() != 0) {
            if (gUnitImprovementsAllowed == true) {
                if (this->mAttribute != 0x10) {
                    EEComplexUnit* theUnit = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0);
                    theUnit->GetStats()->GetImprovementName(theText, this->mAttribute);
                    outString += theText;
                    EEUnitClassImprovementTable* theTable = theUnit->mUnitClass->mImprovements;
                    if ((this->mAttribute != kImprovementRepair ? theTable->mEntries[this->mAttribute].mStepFactor : 0.0f) > 0.0f &&
                        !theUnit->GetPlayer()->IsImprovementMaxed(theUnit->mUnitClass->GetUnitType(), this->mAttribute)) {
                        theLevel = theUnit->GetPlayer()->GetImprovementLevel(theUnit->mUnitClass->mUnitType, this->mAttribute);
                        theUnit->GetStats()->GetImprovementGainText(theText, this->mAttribute, theLevel);
                        outString += theText;
                        outString += ": ";
                        EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(
                            gUnitClassTable[theUnit->mUnitClass->GetUnitType()] ?
                                gUnitClassTable[theUnit->mUnitClass->GetUnitType()]->GetTechnology() : 100000);
                        if (theNode != NULL) {
                            theNode->GetCostString(theText, theUnit->mUnitClass->GetUnitType(),
                                                   theTable->mEntries[this->mAttribute].mCostFactor * theLevel);
                            outString += theText;
                        } else {
                            outString += "No Technology Node Associated With This Object";
                        }
                    } else {
                        outString += " ";
                        gResource->GetString(IDS_AT_MAXIMUM, theText);
                        outString += theText;
                    }
                } else {
                    UIControl::GetHelpString(outString);
                }
            } else {
                gResource->GetString(IDS_UNIT_IMPROVEMENTS_NOT_ALLOWED, theText);
                outString = theText;
            }
        } else {
            theResult = false;
        }
    } else {
        EEComplexUnit* theUnit = static_cast<EEUIFScenarioEditor*>(this->GetForm())->GetFirstSelectedUnit()->GetComplexUnit();
        if (theUnit != NULL) {
            if (theUnit->GetStats()->mImprovement.CanUpgrade(theUnit->GetStats()->mUnitClass, this->mAttribute)) {
                theLevel = theUnit->GetPlayer()->GetImprovementLevel(theUnit->mUnitClass->GetUnitType(), this->mAttribute);
                theUnit->GetStats()->GetImprovementName(theText, this->mAttribute);
                outString = theText;
                theUnit->GetStats()->GetImprovementGainText(theText, this->mAttribute, theLevel);
                outString += " ";
                outString += theText;
            } else {
                outString += " ";
                gResource->GetString(IDS_AT_MAXIMUM, theText);
                outString += theText;
            }
        } else {
            theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x656f5a
// Refreshes the help text when the hovered improvement's level changed
long EEUIBUpgrade::MouseMove(const GE2DPoint& inLocation)
{
    UWideString theText;
    UIForm* theForm = this->GetForm();
    if (gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() != 0) {
        if (this->mAttribute != 0x10) {
            EEComplexUnit* theUnit = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0);
            if (theUnit->mPlayer->GetImprovementLevel(theUnit->mUnitClass->GetUnitType(), this->mAttribute) == this->mLevel)
                return UIButton::MouseMove(inLocation);
            this->mLevel = theUnit->mPlayer->GetImprovementLevel(theUnit->mUnitClass->GetUnitType(), this->mAttribute);
            this->GetHelpString(theText);
        } else {
            UIControl::GetHelpString(theText);
        }
        if (theForm->GetHelpTextLabel() != NULL)
            theForm->GetHelpTextLabel()->SetText(theText);
    }
    return UIButton::MouseMove(inLocation);
}

// FUNCTION: EXE 0x657040
// Shows the hover state only when the improvement can be bought
void EEUIBUpgrade::MouseOver()
{
    EEUnit* theUnit = NULL;
    if (this->GetCanUpgrade(theUnit, NULL))
        UIButton::MouseOver();
}

// FUNCTION: EXE 0x65706a
// Presses the button on a right press, capturing the mouse
long EEUIBUpgrade::MouseRButtonDown()
{
    this->CaptureMouse();
    this->ButtonDown();
    return kErrorOK;
}

// FUNCTION: EXE 0x657081
// On a right release over the button, sells back the improvement of the selected unit type
long EEUIBUpgrade::MouseRButtonUp()
{
    EEUnit* theUnit = NULL;
    if (this->GetRectangle().Contains(this->GetMouseLocation())) {
        if (!this->mScenarioEditor) {
            EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
            if (theGroup != NULL)
                theUnit = theGroup->GetFirstUnit();
        } else {
            theUnit = static_cast<EEUIFScenarioEditor*>(this->GetForm())->GetFirstSelectedUnit();
        }
        if (theUnit != NULL)
            (new EEAUpgradeImprovement(theUnit, this->mAttribute, false, !this->mScenarioEditor))->Dereference();
    }
    this->GetForm()->UpdateHelpString();
    theUnit = NULL;
    if (this->GetCanUpgrade(theUnit, NULL))
        UIButton::ButtonUp();
    this->ReleaseCapturedMouse();
    return kErrorOK;
}

// FUNCTION: EXE 0x65713e
// Marks the button and its cost label dirty
void EEUIBUpgrade::SetDirty(bool inDirty)
{
    if (inDirty == true && this->mCostLabel != NULL)
        this->mCostLabel->SetDirty(true);
    UIControl::SetDirty(inDirty);
}

// FUNCTION: EXE 0x657169
// Shows a new cost text and dims or restores the button when its availability changed
void EEUIBUpgrade::UpdateCost(const UWideString& inText)
{
    if (this->mCostLabel != NULL && inText != this->mCostLabel->GetText(0)) {
        this->mCostLabel->SetText(inText);
        this->SetDirty(true);
    }
    if (this->mDisabled == this->GetDisabled())
        return;
    this->mDisabled = this->GetDisabled();
    if (!this->mDisabled) {
        this->SetBgOpacity(kBgOpacity);
        this->SetBorderOpacity(kBgOpacity * 3.0f);
        this->SetBorderThickness(1);
    } else {
        this->SetBgOpacity(0.0f);
        this->SetBorderThickness(0);
    }
    this->SetDirty(true);
}
