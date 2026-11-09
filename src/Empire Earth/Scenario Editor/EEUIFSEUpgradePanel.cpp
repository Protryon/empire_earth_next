// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEUpgradePanel.cpp (file name inferred: no __FILE__, no RTTI). The unit tab's
// upgrade panel: 0x1a4e99..0x1a5529 (ctor, dtor, ShowUnit). After EEUIFSEUndo.cpp (0x1a4dbe..0x1a4e99), before
// EESEACreateBlankMap.cpp (0x1a5529). Unit flags: the exe module set.
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
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUICPortrait.h"
#include "ee/EEUIFSEUpgradePanel.h"
#include "ee/EEStringIDs.h"
// the shared empty byte string (the original references it at 0x1a4eb4; no other string has its contents)
extern char gEmptyString[];

// FUNCTION: EXE 0x5a4e99
// Builds the unit tab's status panel (portrait, unit name, two status bars, nine attribute rows and the armor
// title), all hidden, and wires them into its status box
EEUIFSEUpgradePanel::EEUIFSEUpgradePanel(unsigned long& ioID, unsigned long& ioIndex,
                                        EEUIFScenarioEditor* inForm)
{
    UILayoutUtility theLayout;
    GEText* theGEText;
    UWideString theText(gEmptyString);
    GERectangle theRectangle;
    this->mStatusBox = new EEUIFMGStatusBox(inForm);
    theRectangle.SetLeft(6.0f);
    theRectangle.SetTop(619.0f);
    theRectangle.SetRight(36.0f);
    theRectangle.SetBottom(649.0f);
    this->mPortrait = new EEUICPortrait(inForm, theRectangle, ioID, ioIndex, false);
    inForm->AddControl(this->mPortrait, 6);
    this->mStatusBox->mC8C = this->mPortrait;
    this->mPortrait->SetDisplayMode(kDisplayModeHidden);
    ioID++;
    ioIndex++;
    GE2DPoint thePosition(this->mPortrait->GetRectangle().GetRight() + 4.0f,
                          this->mPortrait->GetRectangle().GetTop());
    this->mUnitNameLabel = new UITextLabel(theText, gFontTahoma12, ioID, GERectangle(thePosition, 200.0f, 15.0f),
                               inForm, 0, ioIndex, 0);
    theGEText = this->mUnitNameLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.0f, 0.0f, 0.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitNameLabel, 6);
    this->mStatusBox->mC90 = this->mUnitNameLabel;
    this->mUnitNameLabel->SetDisplayMode(kDisplayModeHidden);
    ioID++;
    ioIndex++;

    theLayout.SetLeft(0, 6.0f);
    theLayout.SetRight(0, 57.0f);
    theLayout.SetTop(3, 111.0f);
    theLayout.SetBottom(3, 76.0f);
    theLayout.CalculateRectangle(inForm->GetRectangle(), theRectangle, false);
    this->mLeftStatusBar = new EEUIFMGStatusBar(ioID, ioIndex, &theLayout, 0x400, 0x300, theRectangle, inForm, true, true, false);
    inForm->AddControl(this->mLeftStatusBar, 6);
    this->mStatusBox->mC84 = this->mLeftStatusBar;
    this->mLeftStatusBar->SetDisplayMode(kDisplayModeHidden);
    ioID++;
    ioIndex++;

    theLayout.SetLeft(0, 6.0f);
    theLayout.SetRight(0, 57.0f);
    theLayout.SetTop(3, 74.0f);
    theLayout.SetBottom(3, 39.0f);
    theLayout.CalculateRectangle(inForm->GetRectangle(), theRectangle, false);
    this->mMiddleStatusBar = new EEUIFMGStatusBar(ioID, ioIndex, &theLayout, 0x400, 0x300, theRectangle, inForm, true, true, false);
    inForm->AddControl(this->mMiddleStatusBar, 6);
    this->mStatusBox->mC88 = this->mMiddleStatusBar;
    this->mMiddleStatusBar->SetDisplayMode(kDisplayModeHidden);
    ioID++;
    ioIndex++;

    for (unsigned long i = 0; i < 9; i++) {
        float theX = i < 5 ? 62.0f : 156.0f;
        thePosition.SetY(i < 5 ? (float)i * 20.0f + 634.0f : (float)(i - 5) * 20.0f + 654.0f);
        long theWidth = i < 5 ? 57 : 48;
        theRectangle.SetTop(thePosition.GetY());
        theRectangle.SetLeft(theX);
        theRectangle.SetRight(theRectangle.GetLeft() + theWidth + 19.0f);
        theRectangle.SetBottom(theRectangle.GetTop() + 17.0f);
        this->mAttributes[i] = new EEUIFMGAttribute(gFontTahoma10, inForm, theRectangle, ioIndex, ioID,
                                              GEColor(0.0f, 0.0f, 0.0f), true, 0);
        inForm->AddControl(this->mAttributes[i], 6);
        this->mAttributes[i]->SetDisplayMode(kDisplayModeHidden);
        ioID++;
        ioIndex++;
        thePosition.SetX(theRectangle.GetLeft() + 19.0f);
        this->mLabels[i] = new UITextLabel(theText, gFontTahoma12, ioID,
                                     GERectangle(thePosition, theWidth, 17.0f), inForm, 0, ioIndex, 0);
        theGEText = this->mLabels[i]->GetGEText();
        theGEText->SetFgColor(GEColor(0.0f, 0.0f, 0.0f));
        theGEText->SetIsTransparent(true);
        this->mLabels[i]->SetAlignment(kTextAlignRight);
        inForm->AddControl(this->mLabels[i], 6);
        this->mLabels[i]->SetDisplayMode(kDisplayModeHidden);
        ioID++;
        ioIndex++;
        // NEAR: the original has two arms here; its second indexes the status box from (i - 5) off +0x4c/+0x70
        // (same addresses), shape not found
        if (i < 5)
            this->mStatusBox->SetControls(i, this->mAttributes[i], this->mLabels[i]);
        else
            this->mStatusBox->SetControls(i, this->mAttributes[i], this->mLabels[i]);
    }
    gResource->GetString(IDS_GAME_ARMOR, theText);
    this->mArmorTitleLabel = new UITextLabel(theText, gFontTahoma12, ioID, GERectangle(156.0f, 634.0f, 240.0f, 650.0f),
                               inForm, 0, ioIndex, 0);
    this->mArmorTitleLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mArmorTitleLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.0f, 0.0f, 0.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mArmorTitleLabel, 6);
    this->mStatusBox->mC98 = this->mArmorTitleLabel;
    this->mArmorTitleLabel->SetDisplayMode(kDisplayModeHidden);
    ioID++;
    ioIndex++;
}

// FUNCTION: EXE 0x5a549d
// Destroys the upgrade panel, deleting its status box and all of its controls
EEUIFSEUpgradePanel::~EEUIFSEUpgradePanel()
{
    delete this->mStatusBox;
    delete this->mPortrait;
    delete this->mArmorTitleLabel;
    delete this->mLeftStatusBar;
    delete this->mMiddleStatusBar;
    delete this->mUnitNameLabel;
    for (long i = 0; i < 9; i++) {
        delete this->mAttributes[i];
        delete this->mLabels[i];
    }
}

// FUNCTION: EXE 0x5a551a
// Fills the panel's status box with the given unit's details
void EEUIFSEUpgradePanel::ShowUnit(EEUnit* inUnit)
{
    this->mStatusBox->Fill(inUnit);
}
