// C:\EEX\Empire Earth\Scenario Editor\EESETSTrigger.cpp (file name inferred: no __FILE__; RTTI class EESETSTrigger).
// The trigger screen's trigger sub-screen: 0x19393b..0x196ebe (ctor, its combo box class's inline ctor and
// MouseRButtonUp, the ??_G pair, the dtor, the slots and members in vtable order of their bodies, the trigger's
// GetConditionCount COMDAT, then the trigger events' ??_G). After EESETSObject.cpp (whose range ends with
// vector<UWideString>::operator= 0x1937b1), before EESETSUICBPlayer.cpp (0x196ebe). Unit flags: the exe module set.
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
#include "ee/EESMArea.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESMCondition.h"
#include "ee/EESMTrigger.h"
#include "ee/EESETSTriggerUIComboBox.h"
#include "ee/EESETSTrigger.h"
#include "ee/EEStringIDs.h"
// The last nine controls set their position with SetXY (one inline expansion each): with SetX/SetY pairs the inline
// budget refuses GEColor::operator= at label 0x9e5 (mSecondConditionLabel), which the original expands.
// FUNCTION: EXE 0x59393b
// Builds the trigger sub-screen: if/then labels, two condition and two effect rows (not, combo, and/or,
// delay, remove), scroll buttons and the is-on, condition timer, looping and cinematic check boxes
EESETSTrigger::EESETSTrigger(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    UWideString theText;
    GE2DPoint thePosition(3.0f, 664.0f);
    GEText* theGEText;
    UIEvent* theEvent;
    gResource->GetString(IDS_IF, theText);
    this->mIfLabel = new UITextLabel(theText, inFont, 0x9c4, GERectangle(thePosition, 40.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mIfLabel->SetBgOpacity(0.4f);
    theGEText = this->mIfLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mIfLabel, 9);
    ioIndex++;
    thePosition.SetX(3.0f);
    thePosition.SetY(711.0f);
    gResource->GetString(IDS_THEN, theText);
    this->mThenLabel = new UITextLabel(theText, inFont, 0x9c5, GERectangle(thePosition, 40.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mThenLabel->SetBgOpacity(0.4f);
    theGEText = this->mThenLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mThenLabel, 9);
    ioIndex++;
    thePosition.SetX(0.0f);
    thePosition.SetY(616.0f);
    gResource->GetString(IDS_TRIGGER_STATEMENT, theText);
    this->mTriggerStatementLabel = new UITextLabel(theText, inFont, 0x9c6, GERectangle(thePosition, 150.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mTriggerStatementLabel->SetBgOpacity(0.4f);
    theGEText = this->mTriggerStatementLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTriggerStatementLabel, 9);
    ioIndex++;
    thePosition.SetX(45.0f);
    thePosition.SetY(689.0f);
    gResource->GetString(IDS_DELAY, theText);
    this->mFirstDelayLabel = new UITextLabel(theText, inFont, 0x9c7, GERectangle(thePosition, 45.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mFirstDelayLabel->SetBgOpacity(0.4f);
    theGEText = this->mFirstDelayLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mFirstDelayLabel, 9);
    ioIndex++;
    thePosition.SetX(353.0f);
    thePosition.SetY(689.0f);
    gResource->GetString(IDS_DELAY, theText);
    this->mSecondDelayLabel = new UITextLabel(theText, inFont, 0x9c8, GERectangle(thePosition, 45.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mSecondDelayLabel->SetBgOpacity(0.4f);
    theGEText = this->mSecondDelayLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSecondDelayLabel, 9);
    ioIndex++;
    thePosition.SetX(45.0f);
    thePosition.SetY(641.0f);
    gResource->GetString(IDS_NOT, theText);
    this->mFirstNotLabel = new UITextLabel(theText, inFont, 0x9c9, GERectangle(thePosition, 45.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mFirstNotLabel->SetBgOpacity(0.4f);
    this->mFirstNotLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mFirstNotLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mFirstNotLabel, 9);
    ioIndex++;
    thePosition.SetX(353.0f);
    thePosition.SetY(641.0f);
    gResource->GetString(IDS_NOT, theText);
    this->mSecondNotLabel = new UITextLabel(theText, inFont, 0x9ca, GERectangle(thePosition, 45.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mSecondNotLabel->SetBgOpacity(0.4f);
    this->mSecondNotLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mSecondNotLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSecondNotLabel, 9);
    ioIndex++;
    thePosition.SetX(411.0f);
    thePosition.SetY(557.0f);
    gResource->GetString(IDS_SECONDS, theText);
    this->mSecondsLabel = new UITextLabel(theText, inFont, kSETSTriggerSecondsLabel, GERectangle(thePosition, 100.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mSecondsLabel->SetBgOpacity(0.4f);
    theGEText = this->mSecondsLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSecondsLabel, 9);
    ioIndex++;
    thePosition.SetX(103.0f);
    thePosition.SetY(641.0f);
    this->mFirstConditionLabel = new UITextLabel(theText, inFont, 0x9e4, GERectangle(thePosition, 200.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mFirstConditionLabel->SetBgOpacity(0.4f);
    this->mFirstConditionLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mFirstConditionLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mFirstConditionLabel, 9);
    ioIndex++;
    thePosition.SetX(411.0f);
    thePosition.SetY(641.0f);
    this->mSecondConditionLabel = new UITextLabel(theText, inFont, 0x9e5, GERectangle(thePosition, 200.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mSecondConditionLabel->SetBgOpacity(0.4f);
    this->mSecondConditionLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mSecondConditionLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSecondConditionLabel, 9);
    ioIndex++;
    thePosition.SetX(103.0f);
    thePosition.SetY(689.0f);
    this->mFirstEffectLabel = new UITextLabel(theText, inFont, 0x9e6, GERectangle(thePosition, 200.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mFirstEffectLabel->SetBgOpacity(0.4f);
    this->mFirstEffectLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mFirstEffectLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mFirstEffectLabel, 9);
    ioIndex++;
    thePosition.SetX(411.0f);
    thePosition.SetY(689.0f);
    this->mSecondEffectLabel = new UITextLabel(theText, inFont, 0x9e7, GERectangle(thePosition, 200.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mSecondEffectLabel->SetBgOpacity(0.4f);
    this->mSecondEffectLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mSecondEffectLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSecondEffectLabel, 9);
    ioIndex++;
    theText = "";
    thePosition.SetX(56.0f);
    thePosition.SetY(664.0f);
    this->mFirstNotCheckBox = new UICheckBox(0x9cc, theText, inFont, GERectangle(thePosition, 22.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mFirstNotCheckBox, 9);
    ioIndex++;
    theText = "";
    thePosition.SetX(364.0f);
    thePosition.SetY(664.0f);
    this->mSecondNotCheckBox = new UICheckBox(0x9cd, theText, inFont, GERectangle(thePosition, 22.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mSecondNotCheckBox, 9);
    ioIndex++;
    thePosition.SetX(92.0f);
    thePosition.SetY(664.0f);
    this->mFirstConditionComboBox = new EESETSTriggerUIComboBox(kSETriggerScreenCondition, 0x9ce, 1, std::vector<UWideString>(), gU531930, thePosition, 200.0f, 400,
                                           inForm, ioIndex, 0, 2);
    this->mFirstConditionComboBox->SetTextBoxBgOpacity(0.4f);
    this->mFirstConditionComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mFirstConditionComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mFirstConditionComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mFirstConditionComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mFirstConditionComboBox, 9);
    ioIndex++;
    thePosition.SetX(400.0f);
    thePosition.SetY(664.0f);
    this->mSecondConditionComboBox = new EESETSTriggerUIComboBox(kSETriggerScreenCondition, 0x9cf, 1, std::vector<UWideString>(), gU531930, thePosition, 200.0f, 400,
                                           inForm, ioIndex, 0, 2);
    this->mSecondConditionComboBox->SetTextBoxBgOpacity(0.4f);
    this->mSecondConditionComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mSecondConditionComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mSecondConditionComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mSecondConditionComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mSecondConditionComboBox, 9);
    ioIndex++;
    thePosition.SetX(294.0f);
    thePosition.SetY(664.0f);
    this->mFirstConditionOperatorComboBox = new UIComboBox(0x9d0, 1, std::vector<UWideString>(), gU531930, GERectangle(thePosition, 55.0f, 20.0f), 400,
                              inForm, ioIndex, 0, 2);
    this->mFirstConditionOperatorComboBox->SetTextBoxBgOpacity(0.4f);
    this->mFirstConditionOperatorComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mFirstConditionOperatorComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mFirstConditionOperatorComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mFirstConditionOperatorComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mFirstConditionOperatorComboBox, 9);
    ioIndex++;
    gResource->GetString(IDS_AND, theText);
    this->mFirstConditionOperatorComboBox->AppendLine(theText);
    gResource->GetString(IDS_OR, theText);
    this->mFirstConditionOperatorComboBox->AppendLine(theText);
    thePosition.SetX(602.0f);
    thePosition.SetY(664.0f);
    this->mSecondConditionOperatorComboBox = new UIComboBox(0x9d1, 1, std::vector<UWideString>(), gU531930, GERectangle(thePosition, 55.0f, 20.0f), 400,
                              inForm, ioIndex, 0, 2);
    this->mSecondConditionOperatorComboBox->SetTextBoxBgOpacity(0.4f);
    this->mSecondConditionOperatorComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mSecondConditionOperatorComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mSecondConditionOperatorComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mSecondConditionOperatorComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mSecondConditionOperatorComboBox, 9);
    ioIndex++;
    gResource->GetString(IDS_AND, theText);
    this->mSecondConditionOperatorComboBox->AppendLine(theText);
    gResource->GetString(IDS_OR, theText);
    this->mSecondConditionOperatorComboBox->AppendLine(theText);
    thePosition.SetX(45.0f);
    thePosition.SetY(711.0f);
    this->mFirstDelayEditBox = new UIEditBox(0x9d2, inFont, 4, true, GERectangle(thePosition, 45.0f, 20.0f), inForm, ioIndex, 2, false, false);
    this->mFirstDelayEditBox->SetBgOpacity(0.4f);
    theGEText = this->mFirstDelayEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mFirstDelayEditBox, 9);
    ioIndex++;
    thePosition.SetX(353.0f);
    thePosition.SetY(711.0f);
    this->mSecondDelayEditBox = new UIEditBox(0x9d3, inFont, 4, true, GERectangle(thePosition, 45.0f, 20.0f), inForm, ioIndex, 2, false, false);
    this->mSecondDelayEditBox->SetBgOpacity(0.4f);
    theGEText = this->mSecondDelayEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSecondDelayEditBox, 9);
    ioIndex++;
    thePosition.SetX(92.0f);
    thePosition.SetY(711.0f);
    this->mFirstEffectComboBox = new EESETSTriggerUIComboBox(kSETriggerScreenEffect, 0x9d4, 1, std::vector<UWideString>(), gU531930, thePosition, 200.0f, 400,
                                           inForm, ioIndex, 0, 2);
    this->mFirstEffectComboBox->SetTextBoxBgOpacity(0.4f);
    this->mFirstEffectComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mFirstEffectComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mFirstEffectComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mFirstEffectComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mFirstEffectComboBox, 9);
    ioIndex++;
    thePosition.SetX(400.0f);
    thePosition.SetY(711.0f);
    this->mSecondEffectComboBox = new EESETSTriggerUIComboBox(kSETriggerScreenEffect, 0x9d5, 1, std::vector<UWideString>(), gU531930, thePosition, 200.0f, 400,
                                           inForm, ioIndex, 0, 2);
    this->mSecondEffectComboBox->SetTextBoxBgOpacity(0.4f);
    this->mSecondEffectComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mSecondEffectComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mSecondEffectComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mSecondEffectComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mSecondEffectComboBox, 9);
    ioIndex++;
    thePosition.SetX(294.0f);
    thePosition.SetY(711.0f);
    this->mFirstEffectOperatorComboBox = new UIComboBox(0x9d6, 1, std::vector<UWideString>(), gU531930, GERectangle(thePosition, 55.0f, 20.0f), 400,
                              inForm, ioIndex, 0, 2);
    this->mFirstEffectOperatorComboBox->SetTextBoxBgOpacity(0.4f);
    this->mFirstEffectOperatorComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mFirstEffectOperatorComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mFirstEffectOperatorComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mFirstEffectOperatorComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mFirstEffectOperatorComboBox, 9);
    ioIndex++;
    gResource->GetString(IDS_AND, theText);
    this->mFirstEffectOperatorComboBox->AppendLine(theText);
    gResource->GetString(IDS_OR, theText);
    this->mFirstEffectOperatorComboBox->AppendLine(theText);
    thePosition.SetX(602.0f);
    thePosition.SetY(711.0f);
    this->mSecondEffectOperatorComboBox = new UIComboBox(0x9d7, 1, std::vector<UWideString>(), gU531930, GERectangle(thePosition, 55.0f, 20.0f), 400,
                              inForm, ioIndex, 0, 2);
    this->mSecondEffectOperatorComboBox->SetTextBoxBgOpacity(0.4f);
    this->mSecondEffectOperatorComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mSecondEffectOperatorComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mSecondEffectOperatorComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mSecondEffectOperatorComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mSecondEffectOperatorComboBox, 9);
    ioIndex++;
    gResource->GetString(IDS_AND, theText);
    this->mSecondEffectOperatorComboBox->AppendLine(theText);
    gResource->GetString(IDS_OR, theText);
    this->mSecondEffectOperatorComboBox->AppendLine(theText);
    thePosition.SetX(180.0f);
    thePosition.SetY(616.0f);
    gResource->GetString(IDS_SE_REMOVE, theText);
    this->mRemoveFirstConditionButton = new UIButton(0x9d8, NULL, NULL, NULL, GERectangle(thePosition, 115.0f, 20.0f), inForm, ioIndex, 2);
    this->mRemoveFirstConditionButton->SetBgOpacity(0.4f);
    this->mRemoveFirstConditionButton->SetText(theText, inFont);
    theEvent = new EESEETriggerRemove;
    this->mRemoveFirstConditionButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theGEText = this->mRemoveFirstConditionButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mRemoveFirstConditionButton, 9);
    ioIndex++;
    thePosition.SetX(this->mRemoveFirstConditionButton->GetRectangle().GetRight() + 2.0f);
    thePosition.SetY(616.0f);
    gResource->GetString(IDS_SE_REMOVE, theText);
    this->mRemoveSecondConditionButton = new UIButton(0x9d9, NULL, NULL, NULL, GERectangle(thePosition, 115.0f, 20.0f), inForm, ioIndex, 2);
    this->mRemoveSecondConditionButton->SetBgOpacity(0.4f);
    this->mRemoveSecondConditionButton->SetText(theText, inFont);
    theEvent = new EESEETriggerRemove;
    this->mRemoveSecondConditionButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theGEText = this->mRemoveSecondConditionButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mRemoveSecondConditionButton, 9);
    ioIndex++;
    thePosition.SetX(this->mRemoveSecondConditionButton->GetRectangle().GetRight() + 2.0f);
    thePosition.SetY(616.0f);
    gResource->GetString(IDS_SE_REMOVE, theText);
    this->mRemoveFirstEffectButton = new UIButton(0x9da, NULL, NULL, NULL, GERectangle(thePosition, 115.0f, 20.0f), inForm, ioIndex, 2);
    this->mRemoveFirstEffectButton->SetBgOpacity(0.4f);
    this->mRemoveFirstEffectButton->SetText(theText, inFont);
    theEvent = new EESEETriggerRemove;
    this->mRemoveFirstEffectButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theGEText = this->mRemoveFirstEffectButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mRemoveFirstEffectButton, 9);
    ioIndex++;
    thePosition.SetX(this->mRemoveFirstEffectButton->GetRectangle().GetRight() + 2.0f);
    thePosition.SetY(616.0f);
    theText = UWideString("--------------");
    this->mRemoveSecondEffectButton = new UIButton(0x9db, NULL, NULL, NULL, GERectangle(thePosition, 115.0f, 20.0f), inForm, ioIndex, 2);
    this->mRemoveSecondEffectButton->SetBgOpacity(0.4f);
    this->mRemoveSecondEffectButton->SetText(theText, inFont);
    theEvent = new EESEETriggerRemove;
    this->mRemoveSecondEffectButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theGEText = this->mRemoveSecondEffectButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mRemoveSecondEffectButton, 9);
    ioIndex++;
    thePosition.SetXY(3.0f, 641.0f);
    this->mPrevConditionsButton = new UIButton(kSETSTriggerPrevConditionsButton, inForm->GetManager()->GetSBBitmapLeftNormal(), NULL,
                             inForm->GetManager()->GetSBBitmapLeftPressed(), GERectangle(thePosition, 20.0f, 20.0f), inForm,
                             ioIndex, 0);
    theEvent = new EESEETriggerChangeIndex;
    this->mPrevConditionsButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mPrevConditionsButton, 9);
    ioIndex++;
    thePosition.SetXY(this->mSecondConditionOperatorComboBox->GetRectangle().GetRight() - 20.0f, 641.0f);
    this->mNextConditionsButton = new UIButton(kSETSTriggerNextConditionsButton, inForm->GetManager()->GetSBBitmapRightNormal(), NULL,
                             inForm->GetManager()->GetSBBitmapRightPressed(), GERectangle(thePosition, 20.0f, 20.0f), inForm,
                             ioIndex, 0);
    theEvent = new EESEETriggerChangeIndex;
    this->mNextConditionsButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mNextConditionsButton, 9);
    ioIndex++;
    thePosition.SetXY(3.0f, 689.0f);
    this->mPrevEffectsButton = new UIButton(kSETSTriggerPrevEffectsButton, inForm->GetManager()->GetSBBitmapLeftNormal(), NULL,
                             inForm->GetManager()->GetSBBitmapLeftPressed(), GERectangle(thePosition, 20.0f, 20.0f), inForm,
                             ioIndex, 0);
    theEvent = new EESEETriggerChangeIndex;
    this->mPrevEffectsButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mPrevEffectsButton, 9);
    ioIndex++;
    thePosition.SetXY(this->mSecondConditionOperatorComboBox->GetRectangle().GetRight() - 20.0f, 689.0f);
    this->mNextEffectsButton = new UIButton(kSETSTriggerNextEffectsButton, inForm->GetManager()->GetSBBitmapRightNormal(), NULL,
                             inForm->GetManager()->GetSBBitmapRightPressed(), GERectangle(thePosition, 20.0f, 20.0f), inForm,
                             ioIndex, 0);
    theEvent = new EESEETriggerChangeIndex;
    this->mNextEffectsButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mNextEffectsButton, 9);
    ioIndex++;
    gResource->GetString(IDS_TRIGGER_IS_ON, theText);
    thePosition.SetXY(329.0f, 510.0f);
    this->mIsOnCheckBox = new UICheckBox(0x9e0, theText, inFont, GERectangle(thePosition, 250.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mIsOnCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mIsOnCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mIsOnCheckBox, 9);
    ioIndex++;
    gResource->GetString(IDS_CONDITIONS_TRUE_FOR, theText);
    thePosition.SetXY(329.0f, 535.0f);
    this->mConditionTimerCheckBox = new UICheckBox(0x9e1, theText, inFont, GERectangle(thePosition, 250.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mConditionTimerCheckBox->SetBgOpacity(0.4f);
    theEvent = new EESEETriggerConditionTimer;
    this->mConditionTimerCheckBox->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mConditionTimerCheckBox->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mConditionTimerCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mConditionTimerCheckBox, 9);
    ioIndex++;
    thePosition.SetXY(349.0f, 557.0f);
    this->mConditionDurationEditBox = new UIEditBox(kSETSTriggerConditionDurationEditBox, inFont, 5, true, GERectangle(thePosition, 60.0f, 20.0f), inForm, ioIndex, 2, false, false);
    this->mConditionDurationEditBox->SetBgOpacity(0.4f);
    theGEText = this->mConditionDurationEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mConditionDurationEditBox, 9);
    ioIndex++;
    gResource->GetString(IDS_LOOPING, theText);
    thePosition.SetXY(329.0f, 586.0f);
    this->mLoopingCheckBox = new UICheckBox(0x9e3, theText, inFont, GERectangle(thePosition, 250.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mLoopingCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mLoopingCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mLoopingCheckBox, 9);
    ioIndex++;
    gResource->GetString(IDS_INCLUDE_IN_CINEMATIC, theText);
    thePosition.SetXY(this->mLoopingCheckBox->GetRectangle().GetRight() + 5.0f, 586.0f);
    this->mInCinematicCheckBox = new UICheckBox(0x9e8, theText, inFont, GERectangle(thePosition, 250.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mInCinematicCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mInCinematicCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mInCinematicCheckBox, 9);
    ioIndex++;
    this->mOperatorEvent = new EESEETriggerOperator;
    this->mFirstCondition = 0;
    this->mFirstEffect = 0;
}

// FUNCTION: EXE 0x595c35
// Creates a trigger combo box whose right click opens the picked entry's tab (kind: condition or effect)
// EESETSTriggerUIComboBox::EESETSTriggerUIComboBox (inline, ee/EESETSTriggerUIComboBox.h)

// NEAR: register choice (the original keeps this in edi and the selection in a stack slot) and the screen via the refused vector operator[]
// FUNCTION: EXE 0x595cb5
// Opens the tab of the entry picked in the combo box on a right click and selects it there: triggers,
// conditions (object conditions go to the object tab), effects, objects or areas
long EESETSTriggerUIComboBox::MouseRButtonUp()
{
    EESETriggerScreen* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen();
    unsigned long theSelection;
    unsigned long theIndex;
    UIEvent* theEvent;
    switch (this->mKind) {
    case kSETriggerScreenTrigger:
        theSelection = this->mCurrentSelection;
        if (theSelection != -1) {
            theEvent = theScreen->mTabs[kSETriggerScreenTrigger]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenTrigger]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenTrigger]->Check();
            theScreen->mTabs[kSETriggerScreenTrigger]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenTrigger]->mSelection = theSelection;
            theScreen->ShowTab(kSETriggerTabTrigger);
        }
        break;
    case kSETriggerScreenCondition:
        theSelection = this->mCurrentSelection;
        if (theSelection != -1) {
            unsigned long theCount = gWorld.mScenarioManager->mConditions.size();
            for (theIndex = 0; theIndex < theCount; theIndex++) {
                if (gWorld.mScenarioManager->GetCondition(theIndex)->GetType() != kSMConditionObject)
                    continue;
                if (theIndex != 0 && theSelection >= theIndex) {
                    theEvent = theScreen->mTabs[kSETriggerScreenObject]->GetTriggerEvent(kControlTriggerChecked);
                    if (theEvent != NULL)
                        theEvent->Reference();
                    theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, NULL);
                    theScreen->mTabs[kSETriggerScreenObject]->Check();
                    theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, theEvent);
                    theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - theIndex;
                    theScreen->ShowTab(kSETriggerTabObject);
                    return 0;
                }
                break;
            }
            if (theSelection > 0) {
                theEvent = theScreen->mTabs[kSETriggerScreenCondition]->GetTriggerEvent(kControlTriggerChecked);
                if (theEvent != NULL)
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
        theSelection = this->mCurrentSelection;
        if (theSelection != -1 && theSelection > 0) {
            theEvent = theScreen->mTabs[kSETriggerScreenEffect]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenEffect]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenEffect]->Check();
            theScreen->mTabs[kSETriggerScreenEffect]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenEffect]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabEffect);
        }
        break;
    case kSETriggerScreenObject:
        theSelection = this->mCurrentSelection;
        if (theSelection != -1 && theSelection > 0) {
            theEvent = theScreen->mTabs[kSETriggerScreenObject]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenObject]->Check();
            theScreen->mTabs[kSETriggerScreenObject]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenObject]->mSelection = theSelection - 1;
            theScreen->ShowTab(kSETriggerTabObject);
        }
        break;
    case kSETriggerScreenArea:
        theSelection = this->mCurrentSelection;
        if (theSelection != -1 && theSelection > 1) {
            theEvent = theScreen->mTabs[kSETriggerScreenArea]->GetTriggerEvent(kControlTriggerChecked);
            if (theEvent != NULL)
                theEvent->Reference();
            theScreen->mTabs[kSETriggerScreenArea]->SetTriggerEvent(kControlTriggerChecked, NULL);
            theScreen->mTabs[kSETriggerScreenArea]->Check();
            theScreen->mTabs[kSETriggerScreenArea]->SetTriggerEvent(kControlTriggerChecked, theEvent);
            theScreen->mScreens[kSETriggerScreenArea]->mSelection = theSelection - 2;
            theScreen->ShowTab(kSETriggerTabArea);
        }
        break;
    }
    return 0;
}

// SYNTHETIC: EXE 0x595f98
// (EESETSTriggerUIComboBox's scalar deleting destructor)

// SYNTHETIC: EXE 0x595fc2
// (EESETSTrigger's scalar deleting destructor)

// FUNCTION: EXE 0x595fde
// Destroys the trigger sub-screen's controls and releases its operator event
EESETSTrigger::~EESETSTrigger()
{
    delete this->mIfLabel;
    delete this->mThenLabel;
    delete this->mTriggerStatementLabel;
    delete this->mFirstDelayLabel;
    delete this->mSecondDelayLabel;
    delete this->mFirstNotLabel;
    delete this->mSecondNotLabel;
    delete this->mSecondsLabel;
    delete this->mFirstNotCheckBox;
    delete this->mSecondNotCheckBox;
    delete this->mFirstConditionComboBox;
    delete this->mSecondConditionComboBox;
    delete this->mFirstConditionOperatorComboBox;
    delete this->mSecondConditionOperatorComboBox;
    delete this->mFirstDelayEditBox;
    delete this->mSecondDelayEditBox;
    delete this->mFirstEffectComboBox;
    delete this->mSecondEffectComboBox;
    delete this->mFirstEffectOperatorComboBox;
    delete this->mSecondEffectOperatorComboBox;
    delete this->mRemoveFirstConditionButton;
    delete this->mRemoveSecondConditionButton;
    delete this->mRemoveFirstEffectButton;
    delete this->mRemoveSecondEffectButton;
    delete this->mPrevConditionsButton;
    delete this->mNextConditionsButton;
    delete this->mPrevEffectsButton;
    delete this->mNextEffectsButton;
    delete this->mFirstConditionLabel;
    delete this->mSecondConditionLabel;
    delete this->mFirstEffectLabel;
    delete this->mSecondEffectLabel;
    delete this->mConditionDurationEditBox;
    delete this->mLoopingCheckBox;
    delete this->mIsOnCheckBox;
    delete this->mConditionTimerCheckBox;
    delete this->mInCinematicCheckBox;
    this->mOperatorEvent->Dereference();
}

// FUNCTION: EXE 0x5961d4
// Sorts the scenario's triggers by name
void EESETSTrigger::Sort()
{
    gWorld.mScenarioManager->SortTriggers();
}

// FUNCTION: EXE 0x5961df
// Moves the selected trigger one place down
void EESETSTrigger::MoveDown()
{
    gWorld.mScenarioManager->MoveTriggerDown(this->mSelection);
}

// FUNCTION: EXE 0x5961ee
// Moves the selected trigger one place up
void EESETSTrigger::MoveUp()
{
    gWorld.mScenarioManager->MoveTriggerUp(this->mSelection);
}

// FUNCTION: EXE 0x5961fd
// Deletes the selected trigger and scrolls the condition and effect rows back to the start
void EESETSTrigger::Delete()
{
    this->mFirstCondition = 0;
    this->mFirstEffect = 0;
    gWorld.mScenarioManager->DeleteTrigger(this->mSelection);
}

// FUNCTION: EXE 0x59621a
// Hides the second condition row when the trigger has a single condition, else shows it
void EESETSTrigger::UpdateSecondConditionRow(bool inSingle)
{
    unsigned long theMode = inSingle != true;
    this->mSecondNotLabel->SetDisplayMode(theMode);
    this->mSecondNotCheckBox->SetDisplayMode(theMode);
    this->mSecondConditionComboBox->SetDisplayMode(theMode);
    this->mSecondConditionOperatorComboBox->SetDisplayMode(theMode);
    this->mRemoveSecondConditionButton->SetDisplayMode(theMode);
    this->mSecondConditionLabel->SetDisplayMode(theMode);
}

// FUNCTION: EXE 0x596265
// Hides the second effect row when the trigger has a single effect, else shows it
void EESETSTrigger::UpdateSecondEffectRow(bool inSingle)
{
    unsigned long theMode = inSingle != true;
    this->mSecondDelayLabel->SetDisplayMode(theMode);
    this->mSecondDelayEditBox->SetDisplayMode(theMode);
    this->mSecondEffectComboBox->SetDisplayMode(theMode);
    this->mSecondEffectOperatorComboBox->SetDisplayMode(theMode);
    this->mRemoveSecondEffectButton->SetDisplayMode(theMode);
    this->mSecondEffectLabel->SetDisplayMode(theMode);
}

// FUNCTION: EXE 0x5962b0
// Duplicates the selected trigger and scrolls the condition and effect rows back to the start
void EESETSTrigger::Duplicate()
{
    this->mFirstCondition = 0;
    this->mFirstEffect = 0;
    gWorld.mScenarioManager->DuplicateTrigger(this->mSelection);
}

// FUNCTION: EXE 0x5962cd
// Fills the condition and effect combo boxes with every condition and effect name and hides the
// description and sort button
void EESETSTrigger::Activate()
{
    std::vector<UWideString> theNames;
    unsigned long i;
    this->mFirstConditionComboBox->DeleteAllEntries();
    this->mSecondConditionComboBox->DeleteAllEntries();
    this->mFirstEffectComboBox->DeleteAllEntries();
    this->mSecondEffectComboBox->DeleteAllEntries();
    gWorld.mScenarioManager->GetAllConditionNames(theNames);
    for (i = 0; i < theNames.size(); i++) {
        this->mFirstConditionComboBox->AppendLine(theNames[i]);
        this->mSecondConditionComboBox->AppendLine(theNames[i]);
    }
    theNames.clear();
    gWorld.mScenarioManager->GetAllEffectNames(theNames);
    for (i = 0; i < theNames.size(); i++) {
        this->mFirstEffectComboBox->AppendLine(theNames[i]);
        this->mSecondEffectComboBox->AppendLine(theNames[i]);
    }
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->HideText();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->HideAlphaSort();
}

// NEAR: inline budget (the original expands GetTriggerScreen with operator[] refused at the first site)
// FUNCTION: EXE 0x596415
// Writes the name, description, flags, condition timer and the shown condition and effect rows into the
// selected trigger and detaches the operator events
void EESETSTrigger::Store()
{
    UWideString theText;
    unsigned long theSelection;
    unsigned long theTriggerIndex = this->mSelection;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EESMTrigger* theTrigger = theManager->GetTrigger(theTriggerIndex);
    if (theTrigger != NULL) {
        theTrigger->mName = UWideString(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName());
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->GetAllText(theText);
        theTrigger->mText = theText;
        theTrigger->mIsOn = this->mIsOnCheckBox->GetIsChecked();
        if (this->mConditionTimerCheckBox->GetIsChecked())
            theTrigger->mConditionDuration = this->mConditionDurationEditBox->GetTextAsInt(0);
        else
            theTrigger->mConditionDuration = 0;
        theTrigger->mIsLooping = this->mLoopingCheckBox->GetIsChecked();
        theTrigger->mIsInCinematic = this->mInCinematicCheckBox->GetIsChecked();
        if (this->mFirstConditionComboBox->GetCurrentSelection(theSelection) == kErrorOK && theTrigger->GetCondition(this->mFirstCondition) != NULL)
            theTrigger->SetCondition(this->mFirstCondition, this->mFirstNotCheckBox->GetIsChecked(), theSelection);
        if (this->mFirstCondition < theTrigger->GetConditionCount() - 1) {
            if (this->mSecondConditionComboBox->GetCurrentSelection(theSelection) == kErrorOK && theTrigger->GetCondition(this->mFirstCondition + 1) != NULL)
                theTrigger->SetCondition(this->mFirstCondition + 1, this->mSecondNotCheckBox->GetIsChecked(), theSelection);
            this->mSecondConditionOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        }
        if (this->mFirstEffectComboBox->GetCurrentSelection(theSelection) == kErrorOK && theTrigger->GetEffect(this->mFirstEffect) != NULL)
            theTrigger->SetEffect(this->mFirstEffect, this->mFirstDelayEditBox->GetTextAsInt(0), theSelection);
        unsigned long theIndex = this->mFirstEffect;
        if (theIndex < theTrigger->GetEffectCount() - 1) {
            if (this->mSecondEffectComboBox->GetCurrentSelection(theSelection) == kErrorOK && theTrigger->GetEffect(theIndex + 1) != NULL)
                theTrigger->SetEffect(this->mFirstEffect + 1, this->mSecondDelayEditBox->GetTextAsInt(0), theSelection);
            this->mSecondEffectOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        }
        this->mFirstConditionOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        this->mFirstEffectOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
}

// FUNCTION: EXE 0x59666d
// Stores the trigger, sets the and/or of the changed condition or effect row (a second-row change also
// scrolls on by one) and reloads
void EESETSTrigger::ChangeOperator(unsigned long inControlID)
{
    unsigned long theSelection;
    unsigned long theTriggerIndex = this->mSelection;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EESMTrigger* theTrigger = theManager->GetTrigger(theTriggerIndex);
    this->Store();
    switch (inControlID) {
    case 0x9d0:
        this->mFirstConditionOperatorComboBox->GetCurrentSelection(theSelection);
        theTrigger->SetConditionOperator(this->mFirstCondition, theSelection + 1);
        break;
    case 0x9d1:
        this->mSecondConditionOperatorComboBox->GetCurrentSelection(theSelection);
        theTrigger->SetConditionOperator(this->mFirstCondition + 1, theSelection + 1);
        this->mFirstCondition++;
        break;
    case 0x9d6:
        this->mFirstEffectOperatorComboBox->GetCurrentSelection(theSelection);
        theTrigger->SetEffectOperator(this->mFirstEffect, theSelection + 1);
        break;
    case 0x9d7:
        this->mSecondEffectOperatorComboBox->GetCurrentSelection(theSelection);
        theTrigger->SetEffectOperator(this->mFirstEffect + 1, theSelection + 1);
        this->mFirstEffect++;
        break;
    }
    this->Load();
}

// FUNCTION: EXE 0x596725
// Returns the scenario's trigger names
void EESETSTrigger::GetNames(std::vector<UWideString>& outNames)
{
    gWorld.mScenarioManager->GetTriggerNames(outNames);
}

// FUNCTION: EXE 0x596737
// Hides the trigger's flag check boxes and the condition timer controls
void EESETSTrigger::Hide()
{
    this->mConditionTimerCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mLoopingCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mConditionDurationEditBox->SetDisplayMode(kDisplayModeHidden);
    this->mIsOnCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mSecondsLabel->SetDisplayMode(kDisplayModeHidden);
    this->mInCinematicCheckBox->SetDisplayMode(kDisplayModeHidden);
}

// NEAR: register choice (the original keeps the trigger in ebx and the import in edi) and a 4-byte smaller frame
// FUNCTION: EXE 0x596785
// Loads the selected trigger into the controls: name, description, flags, timer, the shown condition and
// effect pair with their labels and remove buttons, and the scroll buttons
void EESETSTrigger::Load()
{
    UWideString theFormat;
    UWideString theText;
    EESMTrigger* theTrigger;
    bool theSingle;
    EESMTConditionWrapper* theCondition;
    EESMTEffectWrapper* theEffect;
    this->mFirstConditionOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    this->mFirstEffectOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    this->mSecondConditionOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    this->mSecondEffectOperatorComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    unsigned long theTriggerIndex = this->mSelection;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    theTrigger = theManager->GetTrigger(theTriggerIndex);
    if (theTrigger == NULL)
        return;
    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->ShowText();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(theTrigger->mName);
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->SetText(theTrigger->mText);
    this->mIsOnCheckBox->SetChecked(theTrigger->mIsOn);
    this->mConditionDurationEditBox->SetText(theTrigger->mConditionDuration);
    this->mConditionTimerCheckBox->SetChecked(theTrigger->mConditionDuration > 0);
    this->mLoopingCheckBox->SetChecked(theTrigger->mIsLooping);
    this->mInCinematicCheckBox->SetChecked(theTrigger->mIsInCinematic);
    if (this->mFirstCondition >= theTrigger->GetConditionCount() - 1)
        this->mFirstCondition = 0;
    if (this->mFirstEffect >= theTrigger->GetEffectCount() - 1)
        this->mFirstEffect = 0;
    theSingle = theTrigger->GetConditionCount() - 1 == 0;
    this->UpdateSecondConditionRow(theSingle);
    if (!theSingle) {
        theCondition = theTrigger->GetCondition(this->mFirstCondition + 1);
        this->mSecondNotCheckBox->SetChecked(theCondition->mIsNot);
        this->mSecondConditionComboBox->SetCurrentSelection(theCondition->mValue);
        this->ShowOperator(this->mSecondConditionOperatorComboBox, theCondition->mOperator);
    }
    theSingle = theTrigger->GetEffectCount() - 1 == 0;
    this->UpdateSecondEffectRow(theSingle);
    if (!theSingle) {
        theEffect = theTrigger->GetEffect(this->mFirstEffect + 1);
        this->mSecondDelayEditBox->SetText(theEffect->mDelay);
        this->mSecondEffectComboBox->SetCurrentSelection(theEffect->mEffectIndex);
        this->ShowOperator(this->mSecondEffectOperatorComboBox, theEffect->mOperator);
    }
    this->mPrevConditionsButton->SetDisplayMode(this->mFirstCondition != 0);
    this->mPrevEffectsButton->SetDisplayMode(this->mFirstEffect != 0);
    this->mNextConditionsButton->SetDisplayMode(this->mFirstCondition < (long)(theTrigger->GetConditionCount() - 2));
    this->mNextEffectsButton->SetDisplayMode(this->mFirstEffect < (long)(theTrigger->GetEffectCount() - 2));
    theCondition = theTrigger->GetCondition(this->mFirstCondition);
    this->mFirstNotCheckBox->SetChecked(theCondition->mIsNot);
    this->mFirstConditionComboBox->SetCurrentSelection(theCondition->mValue);
    this->ShowOperator(this->mFirstConditionOperatorComboBox, theCondition->mOperator);
    theEffect = theTrigger->GetEffect(this->mFirstEffect);
    this->mFirstDelayEditBox->SetText(theEffect->mDelay);
    this->mFirstEffectComboBox->SetCurrentSelection(theEffect->mEffectIndex);
    this->ShowOperator(this->mFirstEffectOperatorComboBox, theEffect->mOperator);
    gResource->GetString(IDS_CONDITION, theFormat);
    sprintf(theText, "%s %d", (const char*)theFormat, this->mFirstCondition);
    this->mFirstConditionLabel->SetText(theText);
    sprintf(theText, "%s %d", (const char*)theFormat, this->mFirstCondition + 1);
    this->mSecondConditionLabel->SetText(theText);
    gResource->GetString(IDS_EFFECT, theFormat);
    sprintf(theText, "%s %d", (const char*)theFormat, this->mFirstEffect);
    this->mFirstEffectLabel->SetText(theText);
    sprintf(theText, "%s %d", (const char*)theFormat, this->mFirstEffect + 1);
    this->mSecondEffectLabel->SetText(theText);
    gResource->GetString(IDS_DEL, theFormat);
    if (this->mFirstCondition == 0 && theTrigger->GetConditionCount() == 1)
        this->mRemoveFirstConditionButton->SetDisplayMode(kDisplayModeHidden);
    else {
        sprintf(theText, "%s %s", (const char*)theFormat, (const char*)this->mFirstConditionLabel->GetText(0));
        this->mRemoveFirstConditionButton->SetText(theText, gU531934);
        this->mRemoveFirstConditionButton->SetDisplayMode(kDisplayModeShown);
    }
    if (this->mFirstEffect == 0 && theTrigger->GetEffectCount() == 1)
        this->mRemoveFirstEffectButton->SetDisplayMode(kDisplayModeHidden);
    else {
        sprintf(theText, "%s %s", (const char*)theFormat, (const char*)this->mFirstEffectLabel->GetText(0));
        this->mRemoveFirstEffectButton->SetText(theText, gU531934);
        this->mRemoveFirstEffectButton->SetDisplayMode(kDisplayModeShown);
    }
    sprintf(theText, "%s %s", (const char*)theFormat, (const char*)this->mSecondConditionLabel->GetText(0));
    this->mRemoveSecondConditionButton->SetText(theText, gU531934);
    sprintf(theText, "%s %s", (const char*)theFormat, (const char*)this->mSecondEffectLabel->GetText(0));
    this->mRemoveSecondEffectButton->SetText(theText, gU531934);
}

// FUNCTION: EXE 0x596c50
// Adds a new trigger and scrolls the condition and effect rows back to the start
void EESETSTrigger::New()
{
    this->mFirstCondition = 0;
    this->mFirstEffect = 0;
    gWorld.mScenarioManager->NewTrigger();
}

// FUNCTION: EXE 0x596c69
// Stores the trigger, removes the condition or effect of the clicked remove button, steps the rows back
// when the pair ran past the end and reloads
void EESETSTrigger::RemoveEntry(unsigned long inControlID)
{
    unsigned long theIndex;
    unsigned long theTriggerIndex = this->mSelection;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EESMTrigger* theTrigger = theManager->GetTrigger(theTriggerIndex);
    this->Store();
    switch (inControlID) {
    case 0x9d8:
        theTrigger->DeleteCondition(this->mFirstCondition);
        theIndex = this->mFirstCondition;
        if (theIndex + 1 == theTrigger->GetConditionCount() && theIndex > 0)
            this->mFirstCondition = theIndex - 1;
        break;
    case 0x9d9:
        theTrigger->DeleteCondition(this->mFirstCondition + 1);
        theIndex = this->mFirstCondition;
        if (theIndex + 1 == theTrigger->GetConditionCount() && theIndex > 0)
            this->mFirstCondition = theIndex - 1;
        break;
    case 0x9da:
        theTrigger->DeleteEffect(this->mFirstEffect);
        theIndex = this->mFirstEffect;
        if (theIndex + 1 == theTrigger->GetEffectCount() && theIndex > 0)
            this->mFirstEffect = theIndex - 1;
        break;
    case 0x9db:
        theTrigger->DeleteEffect(this->mFirstEffect + 1);
        theIndex = this->mFirstEffect;
        if (theIndex + 1 == theTrigger->GetEffectCount() && theIndex > 0)
            this->mFirstEffect = theIndex - 1;
        break;
    }
    this->Load();
}

// FUNCTION: EXE 0x596d15
// Shows a row's operator in its combo box ('end', and, or) and reattaches the operator event
void EESETSTrigger::ShowOperator(UIComboBox* inComboBox, long inOperator)
{
    UWideString theText;
    switch (inOperator) {
    case kSMTriggerOperatorEnd:
        gResource->GetString(IDS_TRIGGER_EDITOR_END, theText);
        static_cast<UITextLabel*>(inComboBox->GetTextLabel())->SetText(theText);
        inComboBox->SetCurrentSelection(-1);
        break;
    case kSMTriggerOperatorAnd:
        inComboBox->SetCurrentSelection(0);
        break;
    case kSMTriggerOperatorOr:
        inComboBox->SetCurrentSelection(1);
        break;
    }
    inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, this->mOperatorEvent);
}

// FUNCTION: EXE 0x596d94
// Shows the trigger's flag check boxes and, while the timer is checked, the duration box and seconds label
void EESETSTrigger::Show()
{
    if (this->mConditionTimerCheckBox->GetIsChecked() == true) {
        this->mConditionDurationEditBox->SetDisplayMode(kDisplayModeShown);
        this->mSecondsLabel->SetDisplayMode(kDisplayModeShown);
    } else {
        this->mConditionDurationEditBox->SetDisplayMode(kDisplayModeHidden);
        this->mSecondsLabel->SetDisplayMode(kDisplayModeHidden);
    }
    this->mConditionTimerCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mLoopingCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mIsOnCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mInCinematicCheckBox->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x596dff
// Returns the trigger's number of conditions (the address is also bound to EEUnitClass::GetNumResearchTechnologies)
// EESMTrigger::GetConditionCount (inline, ee/EESMTrigger.h)

// SYNTHETIC: EXE 0x596e12
// (EESEETriggerOperator's scalar deleting destructor)

// SYNTHETIC: EXE 0x596e3d
// (EESEETriggerConditionTimer's scalar deleting destructor)

// SYNTHETIC: EXE 0x596e68
// (EESEETriggerChangeIndex's scalar deleting destructor)

// SYNTHETIC: EXE 0x596e93
// (EESEETriggerRemove's scalar deleting destructor)
