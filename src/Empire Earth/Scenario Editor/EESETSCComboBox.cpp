// C:\EEX\Empire Earth\Scenario Editor\EESETSCComboBox.cpp (file name inferred: no __FILE__). The condition screen's
// controls: 0x17daa8..0x17e49e (EESETSConditionControls' ctor, EESETSEScenarioComboBox's ctor + ??_G, EESETSCComboBox's
// ctor + ChangeSelection, EESETSConditionControls' dtor). After EESETSArea.cpp's COMDATs (the ??_G 0x17da35..), before
// EESETSCondition.cpp. Unit flags: the exe module set.
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
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSCComboBox.h"
#include "ee/EESETSConditionControls.h"

// NEAR: the original sets and (dead) tests a temporary flag (0x400) for the slider's GERectangle, ours none
// FUNCTION: EXE 0x57daa8
// Creates the condition page's shared controls (labels, combos, edit boxes, check boxes, radio buttons,
// player pickers, a slider), all hidden, and adds them to the trigger form
EESETSConditionControls::EESETSConditionControls(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    GE2DPoint thePosition;
    unsigned long i;
    GEText* theText;

    for (i = 0; i < 5; i++)
    {
        this->mLabels[i] = new UITextLabel(UWideString(""), inFont, 0x906 + i, GERectangle(thePosition, 150.0f, 20.0f), inForm,
                                     0, ioIndex, 0);
        this->mLabels[i]->SetBgOpacity(0.4f);
        theText = this->mLabels[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        inForm->AddControl(this->mLabels[i], 12);
        ioIndex++;
        this->mLabels[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 3; i++)
    {
        std::vector<UWideString> theTemp1;
        this->mComboBoxes[i] = new EESETSCComboBox(0x933 + i, 1, theTemp1, inFont, thePosition, 0x122, 0x190,
                                             inForm, ioIndex, 0, 2);
        this->mComboBoxes[i]->SetTextBoxBgOpacity(0.4f);
        this->mComboBoxes[i]->SetListBoxAlwaysDirty(true);
        theText = this->mComboBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mComboBoxes[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mComboBoxes[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mComboBoxes[i], 12);
        ioIndex++;
        this->mComboBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 4; i++)
    {
        this->mScenarioComboBoxes[i] = new EESETSEScenarioComboBox(kSETriggerScreenCondition, kSETriggerScreenTrigger, 0x936 + i, 1, std::vector<UWideString>(), inFont,
                                                             thePosition, 290.0f, 0x190, inForm, ioIndex, 0, 2);
        this->mScenarioComboBoxes[i]->SetTextBoxBgOpacity(0.4f);
        this->mScenarioComboBoxes[i]->SetListBoxAlwaysDirty(true);
        theText = this->mScenarioComboBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mScenarioComboBoxes[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mScenarioComboBoxes[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mScenarioComboBoxes[i], 12);
        ioIndex++;
        this->mScenarioComboBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        this->mEditBoxes[i] = new UIEditBox(0x910 + i, inFont, 6, true, GERectangle(thePosition, 60.0f, 20.0f), inForm, ioIndex,
                                      2, false, false);
        this->mEditBoxes[i]->SetBgOpacity(0.4f);
        theText = this->mEditBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        inForm->AddControl(this->mEditBoxes[i], 12);
        ioIndex++;
        this->mEditBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    this->mTextEditBox = new UIEditBox(0x915, gU531930, 0xff, false, GERectangle(thePosition, 120.0f, 20.0f), inForm, ioIndex,
                                 2, false, false);
    this->mTextEditBox->SetBgOpacity(0.4f);
    theText = this->mTextEditBox->GetGEText();
    theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theText->SetIsTransparent(true);
    inForm->AddControl(this->mTextEditBox, 12);
    ioIndex++;
    this->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    for (i = 0; i < 3; i++)
    {
        this->mCheckBoxes[i] = new UICheckBox(0x924 + i, UWideString(""), inFont, GERectangle(thePosition, 250.0f, 20.0f),
                                        inForm, ioIndex, 0, false);
        this->mCheckBoxes[i]->SetBgOpacity(0.4f);
        inForm->AddControl(this->mCheckBoxes[i], 12);
        ioIndex++;
        this->mCheckBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        this->mRadioButtons[i] = new UIRadioButton(0x91a + i, UWideString(""), inFont, GERectangle(thePosition, 150.0f, 20.0f),
                                             inForm, ioIndex, 0, false, false, false);
        this->mRadioButtons[i]->SetBgOpacity(0.4f);
        inForm->AddControl(this->mRadioButtons[i], 12);
        ioIndex++;
        this->mRadioButtons[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        std::vector<UWideString> theTemp2;
        this->mPlayers[i] = new EESETSUICBPlayer(0x92e + i, 1, theTemp2, inFont, thePosition, 0x96, 0x190,
                                           inForm, ioIndex, 0, 2);
        this->mPlayers[i]->SetTextBoxBgOpacity(0.4f);
        this->mPlayers[i]->SetListBoxAlwaysDirty(true);
        theText = this->mPlayers[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mPlayers[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mPlayers[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mPlayers[i], 12);
        ioIndex++;
        this->mPlayers[i]->SetDisplayMode(kDisplayModeHidden);
    }
    this->mSlider = new UISlider(0x942, GERectangle(thePosition, 100.0f, 20.0f), 0, inForm, ioIndex, 2);
    this->mSlider->SetBgOpacity(0.4f);
    inForm->AddControl(this->mSlider, 12);
    ioIndex++;
    this->mSlider->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57e291
// Creates a trigger-screen combo box listing scenario items for the given screen and mode, 20 pixels high
EESETSEScenarioComboBox::EESETSEScenarioComboBox(long inScreen, unsigned long inMode, unsigned long inId,
                                                 unsigned long inTextBoxType, std::vector<UWideString> inData,
                                                 GEFont* inFont, const GE2DPoint& inLocation, float inWidth,
                                                 unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                                 unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, inWidth, 20.0f), inMaxListBoxHeight, inForm,
                 inIndex, inMaxChars, inBorderThickness)
{
    this->mMode = inMode;
    this->mScreen = inScreen;
}

// SYNTHETIC: EXE 0x57e31a
// (EESETSEScenarioComboBox's scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x57e33d
// Creates a condition-page combo box of the given width, 20 pixels high
EESETSCComboBox::EESETSCComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString>& inData,
                                 GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                                 unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                 unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight,
                 inForm, inIndex, inMaxChars, inBorderThickness)
{
}

// FUNCTION: EXE 0x57e3b5
// Changes the selection and refreshes the condition page's controls and generated name
void EESETSCComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    static_cast<EESETriggerScreen*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabTriggers])->GetScreen(kSETriggerScreenCondition)->RefreshControls();
}

// FUNCTION: EXE 0x57e3d7
// Destroys the condition page's shared controls
EESETSConditionControls::~EESETSConditionControls()
{
    unsigned long i;

    for (i = 0; i < 5; i++)
        delete this->mLabels[i];
    for (i = 0; i < 3; i++)
        delete this->mComboBoxes[i];
    for (i = 0; i < 4; i++)
        delete this->mScenarioComboBoxes[i];
    for (i = 0; i < 2; i++)
        delete this->mEditBoxes[i];
    delete this->mTextEditBox;
    for (i = 0; i < 3; i++)
        delete this->mCheckBoxes[i];
    for (i = 0; i < 2; i++)
        delete this->mRadioButtons[i];
    for (i = 0; i < 2; i++)
        delete this->mPlayers[i];
    delete this->mSlider;
}
