// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEElevation.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFSEElevation). The editor's elevation tab: 0x1a098e..0x1a1397 (the ctor, EEUIEElevationTool's ??_G, the tab's
// ??_G, dtor and slots). After the form's unit (0x1a0315..: EEUIFSEBrushGroup), before EEUIFSEESElevation.cpp
// (0x1a1397). Unit flags: the exe module set.
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
#include "ee/EEUIFSEESElevation.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEUIFSEPaintElevation.h"
#include "ee/EEUIFSEElevation.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"

// FUNCTION: EXE 0x5a098e
// Builds the elevation tab: the paint area, the hills / cliffs / cliff paths radio buttons, the titles,
// the elevation and strength sliders and their value label (the strength slider starts hidden)
EEUIFSEElevation::EEUIFSEElevation(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long& ioIndex,
                                   EEUIFScenarioEditor* inForm, GEFont* inFont)
{
    UWideString theText;
    GE2DPoint thePosition(0.0f, 0.0f);
    GEText* theGEText;

    this->mPaint = new EEUIFSEPaintElevation(inScroll, inBrushes, kSEElevationPaint, thePosition, 0x400, 0x25e, inForm, ioIndex);
    ioIndex++;
    inForm->AddControl(static_cast<UIControl*>(this->mPaint), 2);

    gResource->GetString(IDS_PAINT_HILLS, theText);
    thePosition.SetXY(130.0f, 616.0f);
    this->mRaiseButton = new UIRadioButton(kSEElevationRaiseButton, theText, gU531934, GERectangle(thePosition, 140.0f, 20.0f), inForm,
                                     ioIndex, 0, false, true, false);
    this->mRaiseButton->GetTextLabel()->SetBgOpacity(0.4f);
    theGEText = this->mRaiseButton->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_SE_ELEVATION_TOOL);
    this->mRaiseButton->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inForm->AddControl(this->mRaiseButton, 2);
    ioIndex++;

    gResource->GetString(IDS_PAINT_CLIFFS, theText);
    thePosition.SetY(this->mRaiseButton->GetRectangle().GetBottom() + 1.0f);
    this->mLowerButton = new UIRadioButton(kSEElevationLowerButton, theText, gU531934, GERectangle(thePosition, 140.0f, 20.0f), inForm,
                                     ioIndex, 0, false, false, false);
    this->mLowerButton->GetTextLabel()->SetBgOpacity(0.4f);
    theGEText = this->mLowerButton->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mLowerButton->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inForm->AddControl(this->mLowerButton, 2);
    ioIndex++;

    gResource->GetString(IDS_PAINT_CLIFF_PATHS, theText);
    thePosition.SetY(this->mLowerButton->GetRectangle().GetBottom() + 1.0f);
    this->mLevelButton = new UIRadioButton(kSEElevationLevelButton, theText, gU531934, GERectangle(thePosition, 140.0f, 20.0f), inForm,
                                     ioIndex, 0, false, false, false);
    this->mLevelButton->GetTextLabel()->SetBgOpacity(0.4f);
    theGEText = this->mLevelButton->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mLevelButton->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mLevelButton, 2);
    ioIndex++;

    thePosition.SetX(280.0f);
    thePosition.SetY(616.0f);
    gResource->GetString(IDS_SE_MAP_TERRAIN, theText);
    this->mElevationTitle = new UITextLabel(theText, inFont, kSEElevationElevationTitle, GERectangle(thePosition, 60.0f, (float)inFont->GetHeight()),
                                      inForm, 0, ioIndex, 0);
    ioIndex++;
    this->mElevationTitle->SetBgOpacity(0.4f);
    theGEText = this->mElevationTitle->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mElevationTitle->SetAlignment(kTextAlignCenter);
    inForm->AddControl(this->mElevationTitle, 2);

    thePosition.SetY(this->mElevationTitle->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_MAP_ELEVATION, theText);
    this->mStrengthTitle = new UITextLabel(theText, inFont, kSEElevationStrengthTitle,
                                     GERectangle(thePosition, 60.0f, (float)this->mElevationTitle->GetHeight()), inForm, 0,
                                     ioIndex, 0);
    ioIndex++;
    this->mStrengthTitle->SetBgOpacity(0.4f);
    theGEText = this->mStrengthTitle->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mStrengthTitle->SetAlignment(kTextAlignCenter);
    inForm->AddControl(this->mStrengthTitle, 2);

    thePosition.SetX(297.0f);
    thePosition.SetY(732.0f - (float)inFont->GetHeight());
    this->mStrengthLabel = new UITextLabel(UWideString("krtsmMaxElevationLevel"), inFont, kSEElevationStrengthLabel,
                                     GERectangle(thePosition, 20.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    ioIndex++;
    this->mStrengthLabel->SetBgOpacity(0.4f);
    theGEText = this->mStrengthLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mStrengthLabel->SetAlignment(kTextAlignCenter);
    inForm->AddControl(this->mStrengthLabel, 2);

    thePosition.SetY(this->mStrengthTitle->GetRectangle().GetBottom() + 1.0f);
    this->mElevationSlider = new EEUIFSEESElevation(kSEElevationElevationSlider, thePosition,
                                              (unsigned long)(this->mStrengthLabel->GetRectangle().GetTop() -
                                                              (this->mStrengthTitle->GetRectangle().GetBottom() + 1.0f)), inForm, ioIndex);
    ioIndex++;
    this->mElevationSlider->SetRange(0, 0x40);
    this->mElevationSlider->SetCurrentRangeValue(this->mElevationSlider->GetMaxRange() - 0x18);
    this->mElevationSlider->SetPageSize(1);
    this->mElevationSlider->SetBgColor(GEColor(0.2f, 0.2f, 0.2f));
    this->mElevationSlider->SetBgOpacity(0.4f);

    thePosition.SetY(this->mStrengthTitle->GetRectangle().GetBottom() + 1.0f);
    this->mStrengthSlider = new UISlider(kSEElevationStrengthSlider,
                                   GERectangle(thePosition, 20.0f,
                                               this->mStrengthLabel->GetRectangle().GetTop() -
                                                   (this->mStrengthTitle->GetRectangle().GetBottom() + 1.0f)),
                                   1, inForm, ioIndex, 2);
    ioIndex++;
    this->mStrengthSlider->SetRange(0, 0x10);
    this->mStrengthSlider->SetCurrentRangeValue(this->mStrengthSlider->GetMaxRange() - 6);
    this->mStrengthSlider->SetPageSize(1);
    this->mStrengthSlider->SetBgColor(GEColor(0.2f, 0.2f, 0.2f));
    this->mStrengthSlider->SetBgOpacity(0.4f);
    theEvent = new UIESliderChange(this->mStrengthLabel, -4.0f, 0x28);
    this->mStrengthSlider->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    this->mStrengthLabel->SetText(this->mElevationSlider->GetMaxRange() - this->mElevationSlider->GetCurrentRangeValue() - 0x18);
    inForm->AddControl(this->mStrengthSlider, 2);
    inForm->AddControl(this->mElevationSlider, 2);
    this->mStrengthSlider->SetDisplayMode(kDisplayModeHidden);
}

// SYNTHETIC: EXE 0x5a125d
// (EEUIFSEElevation's scalar deleting destructor)

// FUNCTION: EXE 0x5a1279
// Destroys the elevation tab, deleting its paint area, radio buttons, labels and sliders
EEUIFSEElevation::~EEUIFSEElevation()
{
    delete this->mPaint;
    delete this->mRaiseButton;
    delete this->mLowerButton;
    delete this->mLevelButton;
    delete this->mElevationSlider;
    delete this->mStrengthLabel;
    delete this->mElevationTitle;
    delete this->mStrengthTitle;
    delete this->mStrengthSlider;
}

// FUNCTION: EXE 0x5a12fb
// Refreshes the brush highlight at the mouse position while the editor form is the current form
void EEUIFSEElevation::Update()
{
    if (this->mStrengthTitle->GetForm() == this->mStrengthTitle->GetForm()->GetManager()->GetCurrentForm())
        this->MouseMove(gMouse->mPosition);
}

// FUNCTION: EXE 0x5a1329
// Shows the brush highlight on the tile under the mouse while it is inside the game view, else hides it
void EEUIFSEElevation::MouseMove(const GE2DPoint& inLocation)
{
    EETile* theTile;
    long theX;
    long theY;
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mStrengthTitle->GetForm());
    if (gViewport.GetViewRectangle().Contains(inLocation)) {
        EEPointUnitOrTileXY(inLocation, theTile, theX, theY, false);
        theEditor->mBrushGroup->mIsHighlightShown = true;
        theEditor->mBrushGroup->SetHighlight((float)theX, (float)theY, false);
    } else
        theEditor->mBrushGroup->mIsHighlightShown = false;
}
