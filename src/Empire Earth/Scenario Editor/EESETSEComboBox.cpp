// C:\EEX\Empire Earth\Scenario Editor\EESETSEComboBox.cpp (file name inferred: no __FILE__). The effect screen's
// controls: 0x181792..0x182629 (EESETSEffectControls' ctor, EESETSEComboBox's ctor + ChangeSelection + ??_G,
// EESETSEffectControls' dtor). After EESETSConditionType.cpp, before EESETSEffect.cpp. Unit flags: the exe module
// set.
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
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSEffectControls.h"

// FUNCTION: EXE 0x581792
// Creates the effect page's shared controls (labels, combos, edit boxes, radio buttons, sliders, player
// pickers, list boxes), all hidden, and adds them to the trigger form
EESETSEffectControls::EESETSEffectControls(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    GE2DPoint thePosition;
    unsigned long theMode = 0;
    unsigned long i;
    GEText* theText;

    for (i = 0; i < 13; i++)
    {
        this->mLabels[i] = new UITextLabel(UWideString(""), inFont, 0x96a + i, GERectangle(thePosition, 150.0f, 20.0f), inForm,
                                     0, ioIndex, 0);
        this->mLabels[i]->SetBgOpacity(0.4f);
        theText = this->mLabels[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        inForm->AddControl(this->mLabels[i], 13);
        ioIndex++;
        this->mLabels[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        this->mCheckBoxes[i] = new UICheckBox(0x9b5 + i, UWideString(""), inFont, GERectangle(thePosition, 250.0f, 20.0f),
                                        inForm, ioIndex, 0, false);
        this->mCheckBoxes[i]->SetBgOpacity(0.4f);
        inForm->AddControl(this->mCheckBoxes[i], 13);
        ioIndex++;
        this->mCheckBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 4; i++)
    {
        this->mComboBoxes[i] = new UIComboBox(0x997 + i, 1, std::vector<UWideString>(), inFont,
                                        GERectangle(thePosition, 290.0f, 20.0f), 0x190, inForm, ioIndex, 0, 2);
        this->mComboBoxes[i]->SetTextBoxBgOpacity(0.4f);
        this->mComboBoxes[i]->SetListBoxAlwaysDirty(true);
        theText = this->mComboBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mComboBoxes[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mComboBoxes[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mComboBoxes[i], 13);
        ioIndex++;
        this->mComboBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        std::vector<UWideString> theTemp1;
        this->mEffectComboBoxes[i] = new EESETSEComboBox(0x99b + i, 1, theTemp1, inFont, thePosition, 0x122,
                                                   0x190, inForm, ioIndex, 0, 2);
        this->mEffectComboBoxes[i]->SetTextBoxBgOpacity(0.4f);
        this->mEffectComboBoxes[i]->SetListBoxAlwaysDirty(true);
        theText = this->mEffectComboBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mEffectComboBoxes[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mEffectComboBoxes[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mEffectComboBoxes[i], 13);
        ioIndex++;
        this->mEffectComboBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 4; i++)
    {
        switch (i)
        {
        case 0:
            theMode = 0;
            break;
        case 1:
        case 2:
            theMode = 3;
            break;
        case 3:
            theMode = 4;
            break;
        }
        this->mScenarioComboBoxes[i] = new EESETSEScenarioComboBox(kSETriggerScreenEffect, theMode, 0x99d + i, 1, std::vector<UWideString>(), inFont,
                                                             thePosition, 290.0f, 0x190, inForm, ioIndex, 0, 2);
        this->mScenarioComboBoxes[i]->SetTextBoxBgOpacity(0.4f);
        this->mScenarioComboBoxes[i]->SetListBoxAlwaysDirty(true);
        theText = this->mScenarioComboBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mScenarioComboBoxes[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mScenarioComboBoxes[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mScenarioComboBoxes[i], 13);
        ioIndex++;
        this->mScenarioComboBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        this->mEditBoxes[i] = new UIEditBox(0x979 + i, inFont, 6, true, GERectangle(thePosition, 60.0f, 20.0f), inForm, ioIndex,
                                      2, false, false);
        this->mEditBoxes[i]->SetBgOpacity(0.4f);
        theText = this->mEditBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        inForm->AddControl(this->mEditBoxes[i], 13);
        ioIndex++;
        this->mEditBoxes[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        if (i == 0)
            this->mEditBoxes2[i] = new UIEditBox(0x97e + i, gU531930, 5, false, GERectangle(thePosition, 250.0f, 80.0f), inForm,
                                           ioIndex, 2, true, false);
        else
            this->mEditBoxes2[i] = new UIEditBox(0x97e + i, gU531930, 5, false, GERectangle(thePosition, 225.0f, 80.0f), inForm,
                                           ioIndex, 2, true, false);
        this->mEditBoxes2[i]->SetBgOpacity(0.4f);
        theText = this->mEditBoxes2[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        inForm->AddControl(this->mEditBoxes2[i], 13);
        ioIndex++;
        this->mEditBoxes2[i]->SetDisplayMode(kDisplayModeHidden);
    }
    this->mTextEditBox = new UIEditBox(kSETSEComboBoxTextEditBox, gU531930, 0xff, false, GERectangle(thePosition, 250.0f, 20.0f), inForm, ioIndex, 2,
                               false, false);
    this->mTextEditBox->SetBgOpacity(0.4f);
    theText = this->mTextEditBox->GetGEText();
    theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theText->SetIsTransparent(true);
    inForm->AddControl(this->mTextEditBox, 13);
    ioIndex++;
    this->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    this->mEditBox48 = new UIEditBox(0x9ab, gU531930, 0x19, false, GERectangle(thePosition, 100.0f, 20.0f), inForm, ioIndex, 2,
                               false, false);
    this->mEditBox48->SetBgOpacity(0.4f);
    theText = this->mEditBox48->GetGEText();
    theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theText->SetIsTransparent(true);
    inForm->AddControl(this->mEditBox48, 13);
    ioIndex++;
    this->mEditBox48->SetDisplayMode(kDisplayModeHidden);
    for (i = 0; i < 6; i++)
    {
        this->mRadioButtons[i] = new UIRadioButton(0x988 + i, UWideString(""), inFont, GERectangle(thePosition, 125.0f, 20.0f),
                                             inForm, ioIndex, 0, false, false, false);
        this->mRadioButtons[i]->SetBgOpacity(0.4f);
        inForm->AddControl(this->mRadioButtons[i], 13);
        ioIndex++;
        this->mRadioButtons[i]->SetDisplayMode(kDisplayModeHidden);
        if (i & 1)
            ioIndex++;
    }
    for (i = 0; i < 4; i++)
    {
        this->mSliders[i] = new UISlider(0x9a6 + i, GERectangle(thePosition, 100.0f, 20.0f), 0, inForm, ioIndex, 2);
        this->mSliders[i]->SetBgOpacity(0.4f);
        this->mSliders[i]->SetPageSize(1);
        inForm->AddControl(this->mSliders[i], 13);
        ioIndex++;
        this->mSliders[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 3; i++)
    {
        std::vector<UWideString> theTemp2;
        this->mPlayers[i] = new EESETSUICBPlayer(0x992 + i, 1, theTemp2, inFont, thePosition, 0xaf, 0x190,
                                           inForm, ioIndex, 0, 2);
        this->mPlayers[i]->SetTextBoxBgOpacity(0.4f);
        this->mPlayers[i]->SetListBoxAlwaysDirty(true);
        theText = this->mPlayers[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        this->mPlayers[i]->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        this->mPlayers[i]->SetBgOpacity(1.0f);
        inForm->AddControl(this->mPlayers[i], 13);
        ioIndex++;
        this->mPlayers[i]->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0; i < 2; i++)
    {
        std::vector<UWideString> theTemp3;
        this->mListBoxes[i] = new UILBString(0x9bf + i, theTemp3, inFont, true,
                                       GERectangle(thePosition, 150.0f, 150.0f), inForm, ioIndex, 2);
        this->mListBoxes[i]->SetBgOpacity(0.4f);
        theText = this->mListBoxes[i]->GetGEText();
        theText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theText->SetIsTransparent(true);
        inForm->AddControl(this->mListBoxes[i], 13);
        ioIndex++;
        this->mListBoxes[i]->SetDisplayMode(kDisplayModeHidden);
        this->mListBoxes[i]->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
    }
}

// FUNCTION: EXE 0x582445
// Creates an effect-page combo box of the given width, 20 pixels high
EESETSEComboBox::EESETSEComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString>& inData,
                                 GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                                 unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                 unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight,
                 inForm, inIndex, inMaxChars, inBorderThickness)
{
}

// FUNCTION: EXE 0x5824bd
// Changes the selection and refreshes the effect page's generated name
void EESETSEComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    static_cast<EESETriggerScreen*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabTriggers])->GetScreen(kSETriggerScreenEffect)->RefreshControls();
}

// SYNTHETIC: EXE 0x5824df
// (the scalar deleting destructor of EESETSEComboBox's implicit dtor; ICF-shared with EESETSCComboBox's)

// FUNCTION: EXE 0x5824fc
// Destroys the effect page's shared controls
EESETSEffectControls::~EESETSEffectControls()
{
    unsigned long i;

    for (i = 0; i < 13; i++)
        delete this->mLabels[i];
    for (i = 0; i < 4; i++)
        delete this->mComboBoxes[i];
    for (i = 0; i < 2; i++)
        delete this->mEffectComboBoxes[i];
    for (i = 0; i < 4; i++)
        delete this->mScenarioComboBoxes[i];
    delete this->mTextEditBox;
    for (i = 0; i < 2; i++)
        delete this->mEditBoxes2[i];
    for (i = 0; i < 2; i++)
        delete this->mEditBoxes[i];
    delete this->mEditBox48;
    for (i = 0; i < 6; i++)
        delete this->mRadioButtons[i];
    for (i = 0; i < 3; i++)
        delete this->mPlayers[i];
    for (i = 0; i < 4; i++)
        delete this->mSliders[i];
    for (i = 0; i < 2; i++)
        delete this->mCheckBoxes[i];
    for (i = 0; i < 2; i++)
        delete this->mListBoxes[i];
}
