// C:\EEX\Empire Earth\Scenario Editor\EESETSObject.cpp (file name inferred: no __FILE__; RTTI class EESETSObject).
// The trigger screen's object sub-screen: 0x18efb7..0x19393b (the ctor first, as EESETSArea/EESETSTrigger; then the
// out-of-line inlines it refused, the small control classes EESETSOUnitList/EESETSAMouseClick/EESETSALasso/
// EESETSAComboBox/EESETSAEditBox, the ??_G block, the dtor and the slots, the unit type sort's COMDATs and the event
// ??_Gs). After EESETSETSetObjectStance.cpp, before EESETSTrigger.cpp (0x19393b). Unit flags: the exe module set.
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

// Unit action names are the real EEUnitAction::sActionNames table at 0x563668.

// 0x563678: the unit action ids by name (the object screen's action combo box) are EEUnitAction::sActionIDs
#include "ee/EEUnitAction.h"
#include "ee/EEStringIDs.h"
// The unit type combo box's sort (0x19330c, the std::sort instantiation 0x193077..0x19370e): type 0 first, then the
// unit classes by name (provisional name)
struct EESETSOUnitTypeNameLess {
    bool operator()(const unsigned long& inA, const unsigned long& inB) const
    {
        UWideString theA;
        UWideString theB;
        bool theResult;
        if (inA != 0) {
            if (gUnitClassTable[inA] != NULL)
                gResource->GetString(gUnitClassTable[inA]->mNameStringID, theA);
            if (gUnitClassTable[inB] != NULL)
                gResource->GetString(gUnitClassTable[inB]->mNameStringID, theB);
            theResult = theA < theB;
        } else
            theResult = true;
        return theResult;
    }
};

// NEAR: inline budget: the original calls _Ptrit::operator+ out of line in the three GetString loops (the
// counter in the dead ioIndex slot; ours strength-reduces them, two frame slots more) and refuses the last control's
// inlines (SetTextBoxBgOpacity, SetListBoxAlwaysDirty, the GEColor ctor/dtor)
// FUNCTION: EXE 0x58efb7
// Builds the trigger editor's object page: pick mode radios, count, owner, family, class and parent fields,
// the selected unit list and the area, near, attribute, visible-by, state and selected-by filters
EESETSObject::EESETSObject(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    GE2DPoint thePosition;
    UWideString theText;
    std::vector<UWideString> theNames;
    UWideString theFormat;
    unsigned long i;
    long j;

    this->mMouseClick = new EESETSAMouseClick;
    this->mLasso = new EESETSALasso;
    this->mSelectUnits = new EESEUICTriggerSelectUnits(inForm, kSETSObjectSelectUnits, ioIndex, this->mMouseClick, this->mLasso);
    inForm->AddControl(this->mSelectUnits, 0);
    ioIndex++;

    gResource->GetString(IDS_SELECT_ON_MAP, theText);
    thePosition.SetX(329.0f);
    thePosition.SetY(504.0f);
    this->mUnitsRadio = new UIRadioButton(kSETSObjectUnitsRadio, theText, inFont, GERectangle(thePosition, 200.0f, 20.0f), inForm, ioIndex, 0,
                                    false, true, false);
    this->mUnitsRadio->SetBgOpacity(0.4f);
    UIEvent* theEvent = new EESEEObjectDetail;
    this->mUnitsRadio->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
    GEText* theGEText = this->mUnitsRadio->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitsRadio, 10);
    ioIndex++;

    gResource->GetString(IDS_OBJECT_SPECIFICATION, theText);
    thePosition.SetX(329.0f);
    thePosition.SetY(this->mUnitsRadio->GetRectangle().GetBottom() + 1.0f);
    this->mTypeRadio = new UIRadioButton(0x835, theText, inFont, GERectangle(thePosition, 200.0f, 20.0f), inForm, ioIndex, 0,
                                   false, false, false);
    this->mTypeRadio->SetBgOpacity(0.4f);
    theEvent = new EESEEObjectDetail;
    this->mTypeRadio->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mTypeRadio->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTypeRadio, 10);
    ioIndex++;

    thePosition.SetX(329.0f);
    thePosition.SetY(566.0f);
    gResource->GetString(IDS_TRIGGER_EDITOR_MIN, theText);
    this->mMinLabel = new UITextLabel(theText, inFont, 0x839, GERectangle(thePosition, 75.0f, (float)inFont->GetHeight()),
                                inForm, 0, ioIndex, 0);
    this->mMinLabel->SetBgOpacity(0.4f);
    theGEText = this->mMinLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMinLabel, 10);
    ioIndex++;

    thePosition.SetX(341.0f);
    thePosition.SetY(this->mMinLabel->GetRectangle().GetBottom() + 2.0f);
    this->mMinEditBox = new EESETSAEditBox(0x837, inFont, 5, true, thePosition, 50.0f, inFont->GetHeight() + 6, inForm, ioIndex,
                                     2, false, false);
    this->mMinEditBox->SetBgOpacity(0.4f);
    theGEText = this->mMinEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMinEditBox, 10);
    ioIndex++;

    thePosition.SetX(429.0f);
    thePosition.SetY(566.0f);
    gResource->GetString(IDS_TRIGGER_EDITOR_MAX, theText);
    this->mMaxLabel = new UITextLabel(theText, inFont, 0x83a, GERectangle(thePosition, 75.0f, (float)inFont->GetHeight()),
                                inForm, 0, ioIndex, 0);
    this->mMaxLabel->SetBgOpacity(0.4f);
    theGEText = this->mMaxLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMaxLabel, 10);
    ioIndex++;

    thePosition.SetX(441.0f);
    thePosition.SetY(this->mMaxLabel->GetRectangle().GetBottom() + 2.0f);
    this->mMaxEditBox = new EESETSAEditBox(0x838, inFont, 5, true, thePosition, 50.0f, inFont->GetHeight() + 6, inForm, ioIndex,
                                     2, false, false);
    this->mMaxEditBox->SetBgOpacity(0.4f);
    theGEText = this->mMaxEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMaxEditBox, 10);
    ioIndex++;

    gResource->GetString(IDS_DYNAMIC, theText);
    thePosition.SetX(379.0f);
    thePosition.SetY(this->mTypeRadio->GetRectangle().GetBottom() + 2.0f);
    this->mDynamicCheckBox = new UICheckBox(0x844, theText, inFont, GERectangle(thePosition, 100.0f, 20.0f), inForm, ioIndex, 0,
                                 false);
    this->mDynamicCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mDynamicCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mDynamicCheckBox, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(458.0f);
    gResource->GetString(IDS_TRIGGER_EDITOR_OWNER, theText);
    this->mPlayerLabel = new UITextLabel(theText, inFont, 0x83e, GERectangle(thePosition, 200.0f, (float)inFont->GetHeight()),
                                   inForm, 0, ioIndex, 0);
    this->mPlayerLabel->SetBgOpacity(0.4f);
    theGEText = this->mPlayerLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mPlayerLabel, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mPlayerLabel->GetRectangle().GetBottom() + 1.0f);
    this->mPlayer = new EESETSUICBPlayer(0x841, 1, std::vector<UWideString>(), gU531930, thePosition, 200, 400, inForm, ioIndex,
                                   0, 0);
    this->mPlayer->SetTextBoxBgOpacity(0.4f);
    this->mPlayer->SetListBoxAlwaysDirty(true);
    theGEText = this->mPlayer->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mPlayer->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mPlayer->SetBgOpacity(1.0f);
    inForm->AddControl(this->mPlayer, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mPlayer->GetRectangle().GetBottom() + 3.0f);
    gResource->GetString(IDS_FAMILY, theText);
    this->mGroupLabel = new UITextLabel(theText, inFont, 0x83e, GERectangle(thePosition, 200.0f, (float)inFont->GetHeight()),
                                  inForm, 0, ioIndex, 0);
    this->mGroupLabel->SetBgOpacity(0.4f);
    theGEText = this->mGroupLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mGroupLabel, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mGroupLabel->GetRectangle().GetBottom() + 1.0f);
    theNames.resize(0x75);
    for (i = 0; i < 0x75; i++)
        gResource->GetString(i + IDS_UNIT_FAMILY_FIRST, theNames[i]);
    gResource->GetString(IDS_SET, theFormat);
    for (j = 0; j < gUnitSetManager.GetSize(); j++) {
        if (gUnitSetManager.GetSet(j) == NULL || gUnitSetManager.GetSet(j)->mShowInEditor != true)
            continue;
        theNames.push_back(UWideString());
        sprintf(theNames[i], theFormat, (const char*)gUnitSetManager.GetSet(j)->mName);
        this->mUnitSets.push_back(j);
        i++;
    }
    this->mGroupComboBox = new EESETSAComboBox(0x841, 1, theNames, gU531930, thePosition, 200, 400, inForm, ioIndex, 0, 2);
    this->mGroupComboBox->SetTextBoxBgOpacity(0.4f);
    this->mGroupComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mGroupComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mGroupComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mGroupComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mGroupComboBox, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mGroupComboBox->GetRectangle().GetBottom() + 3.0f);
    gResource->GetString(IDS_CLASS, theText);
    this->mTypeLabel = new UITextLabel(theText, inFont, 0x83e, GERectangle(thePosition, 200.0f, (float)inFont->GetHeight()),
                                 inForm, 0, ioIndex, 0);
    this->mTypeLabel->SetBgOpacity(0.4f);
    theGEText = this->mTypeLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTypeLabel, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mTypeLabel->GetRectangle().GetBottom() + 1.0f);
    this->mTypeComboBox = new EESETSAComboBox(0x841, 1, std::vector<UWideString>(), gU531930, thePosition, 200, 400, inForm,
                                        ioIndex, 0, 2);
    this->mTypeComboBox->SetTextBoxBgOpacity(0.4f);
    this->mTypeComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mTypeComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mTypeComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mTypeComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTypeComboBox, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mTypeComboBox->GetRectangle().GetBottom() + 3.0f);
    gResource->GetString(IDS_TRIGGER_EDITOR_USE_OBJECT, theText);
    this->mParentLabel = new UITextLabel(theText, inFont, 0x83e, GERectangle(thePosition, 200.0f, (float)inFont->GetHeight()),
                                   inForm, 0, ioIndex, 0);
    this->mParentLabel->SetBgOpacity(0.4f);
    theGEText = this->mParentLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mParentLabel, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mParentLabel->GetRectangle().GetBottom() + 1.0f);
    this->mParentComboBox = new EESETSAComboBox(0x859, 1, std::vector<UWideString>(), gU531930, thePosition, 200, 400, inForm,
                                          ioIndex, 0, 2);
    this->mParentComboBox->SetTextBoxBgOpacity(0.4f);
    this->mParentComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mParentComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mParentComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mParentComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mParentComboBox, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(458.0f);
    gResource->GetString(IDS_UNIT_SELECTED_LIST, theText);
    this->mUnitListLabel = new UITextLabel(theText, inFont, 0x845, GERectangle(thePosition, 200.0f,
                                     (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mUnitListLabel->SetBgOpacity(0.4f);
    theGEText = this->mUnitListLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitListLabel, 10);
    ioIndex++;

    thePosition.SetX(589.0f);
    thePosition.SetY(this->mUnitListLabel->GetRectangle().GetBottom() + 1.0f);
    this->mUnitList = new EESETSOUnitList(0x836, std::vector<UWideString>(), gU531930, false, thePosition, 200.0f,
                                    (unsigned long)(this->mMaxEditBox->GetRectangle().GetBottom() - thePosition.GetY()), inForm,
                                    ioIndex, 3);
    theEvent = new EESEEObjectUnitSelect;
    this->mUnitList->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    this->mUnitList->SetBgOpacity(0.4f);
    this->mUnitList->SetScrollBarBgOpacity(0.4f);
    theGEText = this->mUnitList->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mUnitList->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mUnitList, 10);
    ioIndex++;

    gResource->GetString(IDS_IN_AREA, theText);
    thePosition.SetX(4.0f);
    thePosition.SetY(616.0f);
    this->mAreaCheck = new UITriStateCheckBox(kSETSObjectAreaCheck, theText, inFont, GERectangle(thePosition, 100.0f, 20.0f), inForm, ioIndex,
                                        0);
    this->mAreaCheck->SetBgOpacity(0.4f);
    theEvent = new EESEEObjectCheck;
    this->mAreaCheck->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mAreaCheck->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mAreaCheck->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAreaCheck, 10);
    ioIndex++;

    thePosition.SetX(114.0f);
    thePosition.SetY(616.0f);
    this->mAreaComboBox = new EESETSEScenarioComboBox(kSETriggerScreenObject, kSETriggerScreenArea, 0x847, 1, std::vector<UWideString>(), gU531930, thePosition, 270.0f,
                                                400, inForm, ioIndex, 0, 2);
    this->mAreaComboBox->SetTextBoxBgOpacity(0.4f);
    this->mAreaComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mAreaComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mAreaComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mAreaComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mAreaComboBox, 10);
    ioIndex++;

    theText = "";
    thePosition.SetX(4.0f);
    thePosition.SetY(652.0f);
    this->mNearCheck = new UITriStateCheckBox(kSETSObjectNearCheck, theText, inFont, GERectangle(thePosition, 22.0f, 20.0f), inForm, ioIndex, 0);
    theEvent = new EESEEObjectCheck;
    this->mNearCheck->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mNearCheck->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mNearCheck, 10);
    ioIndex++;

    thePosition.SetX(this->mNearCheck->GetRectangle().GetRight() + 2.0f);
    theNames.resize(3);
    for (i = 0; i < 3; i++)
        gResource->GetString(i + IDS_TRIGGER_PROXIMITY_FIRST, theNames[i]);
    this->mNearModeComboBox = new UIComboBox(0x849, 1, theNames, gU531930, GERectangle(thePosition, 80.0f, 20.0f), 400, inForm,
                                 ioIndex, 0, 2);
    this->mNearModeComboBox->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mNearModeComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mNearModeComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mNearModeComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mNearModeComboBox, 10);
    ioIndex++;

    thePosition.SetX(114.0f);
    this->mNearObjectComboBox = new UIComboBox(0x849, 1, std::vector<UWideString>(), gU531930, GERectangle(thePosition, 270.0f, 20.0f),
                                 400, inForm, ioIndex, 0, 2);
    this->mNearObjectComboBox->SetTextBoxBgOpacity(0.4f);
    this->mNearObjectComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mNearObjectComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mNearObjectComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mNearObjectComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mNearObjectComboBox, 10);
    ioIndex++;

    gResource->GetString(IDS_HAS_ATTRIBUTE, theText);
    thePosition.SetX(4.0f);
    thePosition.SetY(688.0f);
    this->mHasAttributeCheck = new UITriStateCheckBox(0x84a, theText, inFont, GERectangle(thePosition, 110.0f, 20.0f), inForm, ioIndex,
                                      0);
    this->mHasAttributeCheck->SetBgOpacity(0.4f);
    theEvent = new EESEEObjectCheck;
    this->mHasAttributeCheck->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mHasAttributeCheck->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mHasAttributeCheck->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mHasAttributeCheck, 10);
    ioIndex++;

    thePosition.SetX(4.0f);
    thePosition.SetY(this->mHasAttributeCheck->GetRectangle().GetBottom() + 1.0f);
    theNames.resize(0x4b);
    for (i = 0; i < 0x4b; i++)
        gResource->GetString(i + IDS_TRIGGER_ATTRIBUTE_FIRST, theNames[i]);
    this->mAttributeComboBox = new UIComboBox(0x84b, 1, theNames, gU531930, GERectangle(thePosition, 225.0f, 20.0f), 400, inForm,
                                 ioIndex, 0, 2);
    this->mAttributeComboBox->SetTextBoxBgOpacity(0.4f);
    this->mAttributeComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mAttributeComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mAttributeComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mAttributeComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mAttributeComboBox, 10);
    ioIndex++;

    thePosition.SetX(this->mHasAttributeCheck->GetRectangle().GetRight() + 130.0f);
    thePosition.SetY(this->mHasAttributeCheck->GetRectangle().GetTop());
    gResource->GetString(IDS_TRIGGER_EDITOR_MIN, theText);
    this->mAttributeMinLabel = new UITextLabel(theText, inFont, 0x84b, GERectangle(thePosition, 45.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mAttributeMinLabel->SetBgOpacity(0.4f);
    theGEText = this->mAttributeMinLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAttributeMinLabel, 10);
    ioIndex++;

    thePosition.SetY(this->mAttributeMinLabel->GetRectangle().GetBottom() + 1.0f);
    this->mAttributeMinEditBox = new UIEditBox(0x84a, gU531930, 5, true, GERectangle(thePosition, 45.0f, 20.0f), inForm, ioIndex, 2,
                               false, false);
    this->mAttributeMinEditBox->SetBgOpacity(0.4f);
    theGEText = this->mAttributeMinEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAttributeMinEditBox, 10);
    ioIndex++;

    thePosition.SetX(this->mHasAttributeCheck->GetRectangle().GetRight() + 180.0f);
    thePosition.SetY(this->mHasAttributeCheck->GetRectangle().GetTop());
    gResource->GetString(IDS_TRIGGER_EDITOR_MAX, theText);
    this->mAttributeMaxLabel = new UITextLabel(theText, inFont, 0x84d, GERectangle(thePosition, 45.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mAttributeMaxLabel->SetBgOpacity(0.4f);
    theGEText = this->mAttributeMaxLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAttributeMaxLabel, 10);
    ioIndex++;

    thePosition.SetY(this->mAttributeMaxLabel->GetRectangle().GetBottom() + 1.0f);
    this->mAttributeMaxEditBox = new UIEditBox(0x84c, gU531930, 5, true, GERectangle(thePosition, 45.0f, 20.0f), inForm, ioIndex, 2,
                               false, false);
    this->mAttributeMaxEditBox->SetBgOpacity(0.4f);
    theGEText = this->mAttributeMaxEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAttributeMaxEditBox, 10);
    ioIndex++;

    thePosition.SetX(this->mHasAttributeCheck->GetRectangle().GetRight() + 230.0f);
    thePosition.SetY(this->mHasAttributeCheck->GetRectangle().GetTop());
    theText = "%";
    this->mPercentLabel = new UITextLabel(theText, inFont, 0x856, GERectangle(thePosition, 20.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mPercentLabel->SetBgOpacity(0.4f);
    theGEText = this->mPercentLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mPercentLabel, 10);
    ioIndex++;

    thePosition.SetY(this->mPercentLabel->GetRectangle().GetBottom() + 1.0f);
    theText = "";
    this->mAttributePercentCheckBox = new UICheckBox(0x857, theText, inFont, GERectangle(thePosition, 20.0f, 20.0f), inForm, ioIndex, 0,
                                 false);
    this->mAttributePercentCheckBox->SetBgOpacity(0.4f);
    inForm->AddControl(this->mAttributePercentCheckBox, 10);
    ioIndex++;

    gResource->GetString(IDS_VISIBLE_BY, theText);
    thePosition.SetX(404.0f);
    thePosition.SetY(616.0f);
    this->mVisibleByCheck = new UITriStateCheckBox(kSETSObjectVisibleByCheck, theText, inFont, GERectangle(thePosition, 100.0f, 20.0f), inForm, ioIndex,
                                      0);
    theEvent = new EESEEObjectCheck;
    this->mVisibleByCheck->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mVisibleByCheck->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    this->mVisibleByCheck->SetBgOpacity(0.4f);
    theGEText = this->mVisibleByCheck->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mVisibleByCheck, 10);
    ioIndex++;

    thePosition.SetX(508.0f);
    thePosition.SetY(616.0f);
    this->mPlayerBC = new EESETSUICBPlayer(0x84f, 1, std::vector<UWideString>(), gU531930, thePosition, 0x96, 400, inForm,
                                     ioIndex, 0, 2);
    this->mPlayerBC->SetTextBoxBgOpacity(0.4f);
    this->mPlayerBC->SetListBoxAlwaysDirty(true);
    theGEText = this->mPlayerBC->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mPlayerBC->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mPlayerBC->SetBgOpacity(1.0f);
    inForm->AddControl(this->mPlayerBC, 10);
    ioIndex++;

    gResource->GetString(IDS_HAS_STATE, theText);
    thePosition.SetX(404.0f);
    thePosition.SetY(652.0f);
    this->mHasStateCheck = new UITriStateCheckBox(kSETSObjectHasStateCheck, theText, inFont, GERectangle(thePosition, 100.0f, 20.0f), inForm, ioIndex,
                                      0);
    this->mHasStateCheck->SetBgOpacity(0.4f);
    theEvent = new EESEEObjectCheck;
    this->mHasStateCheck->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mHasStateCheck->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mHasStateCheck->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mHasStateCheck, 10);
    ioIndex++;

    thePosition.SetX(508.0f);
    thePosition.SetY(652.0f);
    this->mActionComboBox = new UIComboBox(0x851, 1, std::vector<UWideString>(), gU531930,
                                     GERectangle(thePosition, 150.0f, 20.0f), 400, inForm, ioIndex, 0, 2);
    this->mActionComboBox->SetTextBoxBgOpacity(0.4f);
    this->mActionComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mActionComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mActionComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mActionComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mActionComboBox, 10);
    ioIndex++;

    gResource->GetString(IDS_SELECTED_BY, theText);
    thePosition.SetX(404.0f);
    thePosition.SetY(688.0f);
    this->mSelectedByCheck = new UITriStateCheckBox(kSETSObjectSelectedByCheck, theText, inFont, GERectangle(thePosition, 100.0f, 20.0f), inForm, ioIndex,
                                      0);
    this->mSelectedByCheck->SetBgOpacity(0.4f);
    theEvent = new EESEEObjectCheck;
    this->mSelectedByCheck->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mSelectedByCheck->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mSelectedByCheck->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSelectedByCheck, 10);
    ioIndex++;

    thePosition.SetX(508.0f);
    thePosition.SetY(688.0f);
    this->mPlayerCC = new EESETSUICBPlayer(0x853, 1, std::vector<UWideString>(), gU531930, thePosition, 0x96, 400, inForm,
                                     ioIndex, 0, 2);
    this->mPlayerCC->SetTextBoxBgOpacity(0.4f);
    this->mPlayerCC->SetListBoxAlwaysDirty(true);
    theGEText = this->mPlayerCC->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mPlayerCC->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mPlayerCC->SetBgOpacity(1.0f);
    inForm->AddControl(this->mPlayerCC, 10);
    ioIndex++;

    this->SetDetailMode(true);
}

// FUNCTION: EXE 0x591759
// Switches between pick-on-map and specification mode and refreshes the controls (inline; the ctor calls it)

// FUNCTION: EXE 0x59176b
// Creates the editor's mouse click selection action (EESEAMouseClick ctor, inline; the ctor calls it)

// FUNCTION: EXE 0x59177d
// Creates the editor's lasso selection action (EESEALasso ctor, inline; the ctor calls it)

// SYNTHETIC: EXE 0x591795
// (EESEALasso's scalar deleting destructor, ICF-shared with EESETSALasso's)

// FUNCTION: EXE 0x5917b7
// Selects a line of the unit list and always fires its selection-changed event (inFireEvent is ignored)
void EESETSOUnitList::ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
{
    UILBString::ChangeSelectionIndex(inIndex, false, inScrollTo);
    this->FireTriggerEvent(kControlTriggerSelectionChanged);
}

// SYNTHETIC: EXE 0x5917db
// (EESETSOUnitList's scalar deleting destructor)

// FUNCTION: EXE 0x5917fe
// Performs the editor's click selection, then refreshes the object page's unit list and name
void EESETSAMouseClick::Virtual4()
{
    EESEAMouseClick::Virtual4();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// FUNCTION: EXE 0x59181f
// Performs the editor's lasso selection, then refreshes the object page's unit list and name
void EESETSALasso::Virtual4()
{
    EESEALasso::Virtual4();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// FUNCTION: EXE 0x591840
// Creates an object page combo box, 20 pixels high
EESETSAComboBox::EESETSAComboBox(unsigned long inID, unsigned long inTextBoxType, const std::vector<UWideString>& inStrings,
                                 GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inMaxListBoxHeight,
                                 UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inID, inTextBoxType, inStrings, inFont, GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight, inForm, inIndex,
                 inMaxChars, inBorderThickness)
{
}

// FUNCTION: EXE 0x5918b8
// Changes the selection, then refreshes the object page's generated name
void EESETSAComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// FUNCTION: EXE 0x5918da
// Accepts the typed count, stores the object page into its scenario object and refreshes the page
void EESETSAEditBox::ProcessEnterKey()
{
    UIEditBox::ProcessEnterKey();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject)->Store();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// SYNTHETIC: EXE 0x591918
// (EESETSAEditBox's scalar deleting destructor)

// SYNTHETIC: EXE 0x591935
// (EESETSObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x591951
// (EESEAMouseClick's scalar deleting destructor, ICF-shared with EESETSAMouseClick's)

// FUNCTION: EXE 0x591973
// Destroys the object page, deleting its controls and releasing its click and lasso actions
EESETSObject::~EESETSObject()
{
    delete this->mNearObjectComboBox;
    delete this->mParentComboBox;
    delete this->mTypeRadio;
    delete this->mMaxEditBox;
    delete this->mMaxLabel;
    delete this->mMinEditBox;
    delete this->mMinLabel;
    delete this->mDynamicCheckBox;
    delete this->mPlayer;
    delete this->mPlayerLabel;
    delete this->mGroupComboBox;
    delete this->mGroupLabel;
    delete this->mTypeComboBox;
    delete this->mTypeLabel;
    delete this->mParentLabel;
    delete this->mUnitList;
    delete this->mUnitListLabel;
    delete this->mAreaCheck;
    delete this->mAreaComboBox;
    delete this->mNearCheck;
    delete this->mHasAttributeCheck;
    delete this->mAttributeComboBox;
    delete this->mVisibleByCheck;
    delete this->mPlayerBC;
    delete this->mHasStateCheck;
    delete this->mActionComboBox;
    delete this->mSelectedByCheck;
    delete this->mPlayerCC;
    delete this->mAttributeMinEditBox;
    delete this->mAttributeMinLabel;
    delete this->mAttributeMaxEditBox;
    delete this->mAttributeMaxLabel;
    delete this->mNearModeComboBox;
    delete this->mSelectUnits;
    delete this->mUnitsRadio;
    delete this->mPercentLabel;
    delete this->mAttributePercentCheckBox;
    this->mLasso->Dereference();
    this->mMouseClick->Dereference();
}

// FUNCTION: EXE 0x591bb4
// Sorts the scenario's objects
void EESETSObject::Sort()
{
    gWorld.mScenarioManager->SortObjects();
}

// FUNCTION: EXE 0x591bbf
// Moves the selected scenario object one place down
void EESETSObject::MoveDown()
{
    gWorld.mScenarioManager->MoveObjectDown(this->mSelection + 1);
}

// FUNCTION: EXE 0x591bd0
// Moves the selected scenario object one place up
void EESETSObject::MoveUp()
{
    gWorld.mScenarioManager->MoveObjectUp(this->mSelection + 1);
}

// FUNCTION: EXE 0x591be1
// Clears the editor's unit selection and deletes the selected scenario object
void EESETSObject::Delete()
{
    gUIManager->mScenarioEditorForm->ClearSelection();
    gWorld.mScenarioManager->DeleteObject(this->mSelection + 1);
}

// FUNCTION: EXE 0x591c06
// Empties the selected unit list (if it exists)
void EESETSObject::ClearUnitList()
{
    if (this->mUnitList != NULL)
        this->mUnitList->DeleteAllEntries();
}

// FUNCTION: EXE 0x591c16
// Clears the editor's unit selection when the object page is left
void EESETSObject::Deactivate()
{
    static_cast<EEUIFScenarioEditor*>(this->mSelectUnits->GetForm())->ClearSelection();
}

// FUNCTION: EXE 0x591c24
// Shows the selected unit list in pick-on-map mode, else the owner, family, class, parent and count fields
void EESETSObject::Refresh()
{
    if (this->mIsSelectOnMap) {
        this->mPlayer->SetDisplayMode(kDisplayModeHidden);
        this->mPlayerLabel->SetDisplayMode(kDisplayModeHidden);
        this->mTypeLabel->SetDisplayMode(kDisplayModeHidden);
        this->mTypeComboBox->SetDisplayMode(kDisplayModeHidden);
        this->mParentLabel->SetDisplayMode(kDisplayModeHidden);
        this->mParentComboBox->SetDisplayMode(kDisplayModeHidden);
        this->mGroupComboBox->SetDisplayMode(kDisplayModeHidden);
        this->mGroupLabel->SetDisplayMode(kDisplayModeHidden);
        this->mMinLabel->SetDisplayMode(kDisplayModeHidden);
        this->mMaxLabel->SetDisplayMode(kDisplayModeHidden);
        this->mMinEditBox->SetDisplayMode(kDisplayModeHidden);
        this->mMaxEditBox->SetDisplayMode(kDisplayModeHidden);
        this->mDynamicCheckBox->SetDisplayMode(kDisplayModeHidden);
        this->mUnitList->SetDisplayMode(kDisplayModeShown);
        this->mUnitListLabel->SetDisplayMode(kDisplayModeShown);
    } else {
        this->mPlayer->SetDisplayMode(kDisplayModeShown);
        this->mPlayerLabel->SetDisplayMode(kDisplayModeShown);
        this->mTypeLabel->SetDisplayMode(kDisplayModeShown);
        this->mTypeComboBox->SetDisplayMode(kDisplayModeShown);
        this->mParentLabel->SetDisplayMode(kDisplayModeShown);
        this->mParentComboBox->SetDisplayMode(kDisplayModeShown);
        this->mGroupComboBox->SetDisplayMode(kDisplayModeShown);
        this->mGroupLabel->SetDisplayMode(kDisplayModeShown);
        this->mMinLabel->SetDisplayMode(kDisplayModeShown);
        this->mMaxLabel->SetDisplayMode(kDisplayModeShown);
        this->mMinEditBox->SetDisplayMode(kDisplayModeShown);
        this->mMaxEditBox->SetDisplayMode(kDisplayModeShown);
        this->mDynamicCheckBox->SetDisplayMode(kDisplayModeShown);
        this->mUnitList->SetDisplayMode(kDisplayModeHidden);
        this->mUnitListLabel->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x591d52
// Duplicates the selected scenario object
void EESETSObject::Duplicate()
{
    gWorld.mScenarioManager->DuplicateObject(this->mSelection + 1);
}

// NEAR: inline budget: the original expands GetPlayerScreen with its operator[] out of line (ours refuses the
// accessor itself)
// FUNCTION: EXE 0x591d63
// Fills the page's player, area and state lists; the class list is built on the first call only
void EESETSObject::Activate()
{
    std::vector<UWideString> theNames;
    UWideString theName;
    unsigned long i;
    unsigned long thePlayer =
        static_cast<EEUIFScenarioEditor*>(this->mAreaCheck->GetForm())->GetPlayerScreen()->GetPlayerCount();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->HideText();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowAlphaSort();
    this->mPlayerBC->mPlayerCount = thePlayer;
    this->mPlayerBC->SetFlags(0xc6);
    this->mPlayerCC->mPlayerCount = thePlayer;
    this->mPlayerCC->SetFlags(0x82);
    this->mPlayer->mPlayerCount = thePlayer;
    this->mPlayer->SetFlags(0xe7);
    this->mAreaComboBox->DeleteAllEntries();
    gWorld.mScenarioManager->GetAreaNames(theNames);
    for (i = 0; i < theNames.size(); i++)
        this->mAreaComboBox->AppendLine(theNames[i]);
    // The class list is built on the first call for this page, which is recreated with every scenario editor
    // (a run-wide flag left a later editor session with an empty list).
    if (this->mUnitTypes.empty()) {
        this->mTypeComboBox->DeleteAllEntries();
        this->mUnitTypes.push_back(0);
        for (i = 1; i < 0x367; i++) {
            EEUnitClass* theClass = gUnitClassTable[i];
            if (theClass != NULL && theClass->mCategory != kUnitCategoryProjectile && theClass->mCategory != kUnitCategoryMissile && theClass->mIsScenarioSelectable == true)
                this->mUnitTypes.push_back(i);
        }
        std::sort(this->mUnitTypes.begin(), this->mUnitTypes.end(), EESETSOUnitTypeNameLess());
        gResource->GetString(IDS_ANY_CLASS, theName);
        this->mTypeComboBox->AppendLine(theName);
        for (i = 1; i < this->mUnitTypes.size(); i++) {
            gResource->GetString(gUnitClassTable[this->mUnitTypes[i]]->mNameStringID, theName);
            this->mTypeComboBox->AppendLine(theName);
        }
    }
    this->mActionComboBox->DeleteAllEntries();
    for (i = 0; i < 0x3e82; i++) {
        switch (i) {
        case 1000:
        case 1002:
        case 1003:
        case 1006:
        case 1008:
        case 1009:
        case 1012:
        case 1013:
        case 1017:
        case 2001:
        case 2004:
        case 2011:
        case 2017:
        case 2021:
        case 4002:
        case 4004:
        case 4012:
        case 4026:
        case 5000:
        case 6001:
        case 6002:
        case 10003:
        case 15000:
        case 15001: {
            std::map<unsigned long, UString>::iterator it = EEUnitAction::sActionNames.find(i);
            if (it != EEUnitAction::sActionNames.end())
                this->mActionComboBox->AppendLine(UWideString(it->second));
            break;
        }
        }
    }
}

// NEAR: the original hoists atoi into ebx, calls the vector members out of line (ICF aliases) and
// _Ptrit::operator+ for the group/action lookups
// FUNCTION: EXE 0x5920f1
// Writes the page into the selected scenario object: name, units, min/max counts (clamped to each other),
// owner, family, class, parent, the filter check boxes and their values
void EESETSObject::Store()
{
    std::vector<unsigned long> theUnits;
    UString theActionName;
    UWideString theName;
    EESMObject* theObject = gWorld.GetScenarioManager()->GetScenarioObject(this->mSelection + 1);
    theName = gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName();
    theObject->SetName(theName);
    theObject->mIsSelectOnMap = this->mUnitsRadio->GetIsChecked();
    theObject->SetUnits(this->mUnitIDs);
    if (atoi(this->mMaxEditBox->GetText(0)) < atoi(this->mMinEditBox->GetText(0)))
        this->mMaxEditBox->SetText(atoi(this->mMinEditBox->GetText(0)));
    else if (atoi(this->mMinEditBox->GetText(0)) > atoi(this->mMaxEditBox->GetText(0)))
        this->mMinEditBox->SetText(atoi(this->mMaxEditBox->GetText(0)));
    theObject->mMinCount = atoi(this->mMinEditBox->GetText(0));
    theObject->mMaxCount = atoi(this->mMaxEditBox->GetText(0));
    theObject->mIsDynamic = this->mDynamicCheckBox->GetIsChecked();
    theObject->mOwner = this->mPlayer->GetSelectedPlayer();
    unsigned long theGroup;
    this->mGroupComboBox->GetCurrentSelection(theGroup);
    if (theGroup >= 0x75)
        theGroup = *(this->mUnitSets.begin() + (theGroup - 0x75)) + 0x75;
    theObject->mFamily = theGroup;
    unsigned long theType;
    this->mTypeComboBox->GetCurrentSelection(theType);
    if (theType == -1)
        theType = 0;
    theObject->mClass = this->mUnitTypes[theType];
    unsigned long theParent;
    this->mParentComboBox->GetCurrentSelection(theParent);
    theObject->SetParent(theParent);
    theObject->mInAreaCheck = this->mAreaCheck->GetCheckState();
    theObject->mNearCheck = this->mNearCheck->GetCheckState();
    theObject->mHasAttributeCheck = this->mHasAttributeCheck->GetCheckState();
    theObject->mVisibleByCheck = this->mVisibleByCheck->GetCheckState();
    theObject->mHasStateCheck = this->mHasStateCheck->GetCheckState();
    theObject->mSelectedByCheck = this->mSelectedByCheck->GetCheckState();
    unsigned long theArea;
    this->mAreaComboBox->GetCurrentSelection(theArea);
    if (theArea != -1)
        theObject->mArea = theArea;
    else
        theObject->mInAreaCheck = 0;
    this->mNearModeComboBox->GetCurrentSelection(theObject->mNearMode);
    unsigned long theValue;
    this->mNearObjectComboBox->GetCurrentSelection(theValue);
    if (theValue != -1)
        theObject->mNearObject = theValue;
    else
        theObject->mNearCheck = 0;
    this->mAttributeComboBox->GetCurrentSelection(theObject->mAttribute);
    if (this->mAttributeMaxEditBox->GetTextAsInt(0) < this->mAttributeMinEditBox->GetTextAsInt(0))
        this->mAttributeMaxEditBox->SetText(this->mAttributeMinEditBox->GetTextAsInt(0));
    else if (this->mAttributeMinEditBox->GetTextAsInt(0) > this->mAttributeMaxEditBox->GetTextAsInt(0))
        this->mAttributeMinEditBox->SetText(this->mAttributeMaxEditBox->GetTextAsInt(0));
    theObject->mAttributeMin = this->mAttributeMinEditBox->GetTextAsInt(0);
    theObject->mAttributeMax = this->mAttributeMaxEditBox->GetTextAsInt(0);
    theObject->mIsAttributePercent = this->mAttributePercentCheckBox->GetIsChecked();
    theObject->mVisibleByPlayer = this->mPlayerBC->GetSelectedPlayer();
    unsigned long theAction;
    this->mActionComboBox->GetCurrentSelection(theAction);
    theActionName = *(this->mActionComboBox->GetListBox()->GetDataArray().begin() + theAction);
    theObject->mState = EEUnitAction::sActionIDs.find(theActionName)->second;
    theObject->mSelectedByPlayer = this->mPlayerCC->GetSelectedPlayer();
}

// FUNCTION: EXE 0x5924e1
// Returns the names of the user-defined scenario objects
void EESETSObject::GetNames(std::vector<UWideString>& outNames)
{
    gWorld.mScenarioManager->GetUserObjectNames(outNames);
}

// FUNCTION: EXE 0x5924f3
// Returns whether the object's units are picked on the map
bool EESETSObject::GetIsSelectOnMap()
{
    return this->mUnitsRadio->GetIsChecked();
}

// FUNCTION: EXE 0x5924fd
// Hides the page's mode radios, count, owner, family, class and parent fields and the unit list
void EESETSObject::Hide()
{
    this->mUnitsRadio->SetDisplayMode(kDisplayModeHidden);
    this->mTypeRadio->SetDisplayMode(kDisplayModeHidden);
    this->mMinEditBox->SetDisplayMode(kDisplayModeHidden);
    this->mMaxEditBox->SetDisplayMode(kDisplayModeHidden);
    this->mMinLabel->SetDisplayMode(kDisplayModeHidden);
    this->mMaxLabel->SetDisplayMode(kDisplayModeHidden);
    this->mDynamicCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayer->SetDisplayMode(kDisplayModeHidden);
    this->mGroupLabel->SetDisplayMode(kDisplayModeHidden);
    this->mGroupComboBox->SetDisplayMode(kDisplayModeHidden);
    this->mTypeLabel->SetDisplayMode(kDisplayModeHidden);
    this->mTypeComboBox->SetDisplayMode(kDisplayModeHidden);
    this->mParentLabel->SetDisplayMode(kDisplayModeHidden);
    this->mParentComboBox->SetDisplayMode(kDisplayModeHidden);
    this->mUnitListLabel->SetDisplayMode(kDisplayModeHidden);
    this->mUnitList->SetDisplayMode(kDisplayModeHidden);
}

// NEAR: inline budget (begin()/operator+ out of line in the original) and the local vectors' frame slots
// FUNCTION: EXE 0x5925a2
// Fills the page from the selected scenario object: object lists, units selected on the map, counts, owner,
// family, class, filters and state (a parent past the list selects the first entry)
void EESETSObject::Load()
{
    std::vector<UWideString> theNames;
    std::vector<UWideString> theActions;
    std::vector<unsigned long> theUnits;
    unsigned long i;
    this->mNearObjectComboBox->DeleteAllEntries();
    this->mParentComboBox->DeleteAllEntries();
    gWorld.mScenarioManager->GetObjectNames(theNames);
    for (i = 0; i < theNames.size(); i++) {
        this->mNearObjectComboBox->AppendLine(theNames[i]);
        this->mParentComboBox->AppendLine(theNames[i]);
    }
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mSelection + 1);
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(theObject->mName);
    if (theObject->mIsSelectOnMap)
        this->mUnitsRadio->Check();
    else
        this->mTypeRadio->Check();
    theObject->GetUnits(theUnits);
    static_cast<EEUIFScenarioEditor*>(this->mAreaCheck->GetForm())->ClearSelection();
    this->mUnitIDs.clear();
    for (i = 0; i < theUnits.size(); i++) {
        RTSUnit* theUnit = gWorld.GetUnit(theUnits[i]);
        if (theUnit != NULL)
            (static_cast<EEUIFScenarioEditor*>(this->mAreaCheck->GetForm()))->SelectUnit(static_cast<EEUnit*>(theUnit));
    }
    this->mMinEditBox->SetText(theObject->mMinCount);
    this->mMaxEditBox->SetText(theObject->mMaxCount);
    this->mDynamicCheckBox->SetChecked(theObject->mIsDynamic);
    this->mPlayer->SetSelectedPlayer(theObject->mOwner);
    unsigned long theGroup = theObject->mFamily;
    if (theGroup >= 0x75) {
        if (std::find(this->mUnitSets.begin(), this->mUnitSets.end(), theObject->mFamily - 0x75) != this->mUnitSets.end())
            theGroup = (std::find(this->mUnitSets.begin(), this->mUnitSets.end(), theObject->mFamily - 0x75) - this->mUnitSets.begin()) +
                       0x75;
        else
            theGroup = 0;
    }
    if (theGroup < this->mGroupComboBox->GetListBox()->GetLineCount())
        this->mGroupComboBox->SetCurrentSelection(theGroup);
    else
        this->mGroupComboBox->SetCurrentSelection(0);
    this->mTypeComboBox->SetCurrentSelection(0);
    for (unsigned long t = 0; t < this->mUnitTypes.size(); t++) {
        if (this->mUnitTypes[t] == theObject->mClass) {
            this->mTypeComboBox->SetCurrentSelection(t);
            break;
        }
    }
    this->mAreaCheck->SetCheckState(theObject->mInAreaCheck);
    this->mNearCheck->SetCheckState(theObject->mNearCheck);
    this->mHasAttributeCheck->SetCheckState(theObject->mHasAttributeCheck);
    this->mVisibleByCheck->SetCheckState(theObject->mVisibleByCheck);
    this->mHasStateCheck->SetCheckState(theObject->mHasStateCheck);
    this->mSelectedByCheck->SetCheckState(theObject->mSelectedByCheck);
    if (theObject->GetArea() < this->mAreaComboBox->GetListBox()->GetLineCount())
        this->mAreaComboBox->SetCurrentSelection(theObject->mArea);
    else if (this->mAreaComboBox->GetListBox()->GetLineCount() > 0)
        this->mAreaComboBox->SetCurrentSelection(0);
    this->mNearModeComboBox->SetCurrentSelection(theObject->mNearMode);
    if (theObject->GetNearObject() < this->mNearObjectComboBox->GetListBox()->GetLineCount())
        this->mNearObjectComboBox->SetCurrentSelection(theObject->mNearObject);
    else if (this->mNearObjectComboBox->GetListBox()->GetLineCount() > 0)
        this->mNearObjectComboBox->SetCurrentSelection(0);
    if (theObject->GetParent() < this->mParentComboBox->GetListBox()->GetLineCount())
        this->mParentComboBox->SetCurrentSelection(theObject->mParent);
    else if (this->mParentComboBox->GetListBox()->GetLineCount() > 0)
        this->mParentComboBox->SetCurrentSelection(0);   // (the parent combo, not the near-object one)
    this->mAttributeComboBox->SetCurrentSelection(theObject->mAttribute);
    this->mAttributeMinEditBox->SetText(theObject->mAttributeMin);
    this->mAttributeMaxEditBox->SetText(theObject->mAttributeMax);
    this->mAttributePercentCheckBox->SetChecked(theObject->mIsAttributePercent);
    this->mPlayerBC->SetSelectedPlayer(theObject->mVisibleByPlayer);
    theActions = this->mActionComboBox->GetListBox()->GetDataArray();
    for (i = 0; i < theActions.size(); i++) {
        unsigned long theAction = theObject->mState;
        std::map<unsigned long, UString>::iterator it = EEUnitAction::sActionNames.find(theAction);
        if (it != EEUnitAction::sActionNames.end() && theActions[i] == it->second)
            break;
    }
    this->mActionComboBox->SetCurrentSelection(i < theActions.size() ? i : 0);
    this->mPlayerCC->SetSelectedPlayer(theObject->mSelectedByPlayer);
    this->UpdateSelection();
}

// FUNCTION: EXE 0x592ac8
// Adds a new scenario object
void EESETSObject::New()
{
    gWorld.mScenarioManager->NewObject();
}

// NEAR: inline budget (the original calls mSelectedUnits.begin()/operator+ out of line, frame 0xb0); the
// selected-unit counts compare unsigned in the original (GetSelectedUnitCount may return unsigned long)
// FUNCTION: EXE 0x592ad3
// Lists the editor's selected units (pick-on-map mode) and regenerates an automatic "<...>" object name
// from the units, else from the parent, class, family or owner
void EESETSObject::UpdateSelection()
{
    UWideString theName;
    UWideString theTypeName;
    UWideString theSuffix;
    UWideString theNumber;
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mSelectUnits->GetForm());
    if (this->mUnitsRadio->GetIsChecked()) {
        this->mUnitList->DeleteAllEntries();
        unsigned long theCount = theEditor->GetSelectedUnitCount();
        this->mUnitIDs.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++) {
            gResource->GetString((theEditor->mSelectedUnits[i])->mUnitClass->mNameStringID, theName);
            this->mUnitList->AppendLine(theName);
            this->mUnitIDs[i] = (theEditor->mSelectedUnits[i])->GetUnitID();
        }
    }
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mSelection + 1);
    if (theObject != NULL) {
        theName = theObject->mName;
        if (theName.GetLength() > 0 && theName[0] == '<') {
            if (this->mUnitsRadio->GetIsChecked() == true) {
                if (theEditor->GetSelectedUnitCount() > 0) {
                    gResource->GetString((theEditor->mSelectedUnits[0])->mUnitClass->mNameStringID, theTypeName);
                    sprintf(theNumber, "%d", (theEditor->mSelectedUnits[0])->mUnitID % 1000);
                    theTypeName = theTypeName + theNumber;
                } else
                    gResource->GetString(IDS_NONE, theTypeName);
                if (theEditor->GetSelectedUnitCount() > 1)
                    theTypeName = theTypeName + "+";
            } else {
                unsigned long theSelection;
                this->mParentComboBox->GetCurrentSelection(theSelection);
                if (theSelection == 0) {
                    UIComboBox* theComboBox = this->mTypeComboBox;
                    theComboBox->GetCurrentSelection(theSelection);
                    if (theSelection == 0) {
                        theComboBox = this->mGroupComboBox;
                        theComboBox->GetCurrentSelection(theSelection);
                    }
                    if (theSelection != 0)
                        theTypeName = theComboBox->GetListBox()->GetData(theSelection);
                    else {
                        this->mPlayer->GetCurrentSelection(theSelection);
                        theTypeName = *(this->mPlayer->GetListBox()->GetDataArray().begin() + theSelection);
                        theTypeName += " ";
                        gResource->GetString(IDS_SE_UNIT_GROUP_UNITS, theSuffix);
                        theTypeName += theSuffix;
                    }
                } else
                    theTypeName = "#:" + gWorld.mScenarioManager->GetScenarioObject(theSelection)->mName;
                if (this->mDynamicCheckBox->GetIsChecked() == true)
                    theTypeName = theTypeName + "*";
            }
            theName = "<" + theTypeName + ">";
            theObject->SetName(theName);
            gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(theObject->mName);
        }
    }
}

// FUNCTION: EXE 0x592ee0
// Shows or hides the controls of the filter check box that changed (area, near, attribute, visible by, ...)
void EESETSObject::CheckChanged(UIEntity* inCheckBox)
{
    unsigned long theMode = static_cast<UITriStateCheckBox*>(inCheckBox)->GetCheckState() != 0;
    switch (static_cast<UIControl*>(inCheckBox)->GetId()) {
    case kSETSObjectAreaCheck:
        this->mAreaComboBox->SetDisplayMode(theMode);
        break;
    case kSETSObjectNearCheck:
        this->mNearObjectComboBox->SetDisplayMode(theMode);
        break;
    case 0x84a:
        this->mAttributeComboBox->SetDisplayMode(theMode);
        this->mAttributeMinEditBox->SetDisplayMode(theMode);
        this->mAttributeMinLabel->SetDisplayMode(theMode);
        this->mAttributeMaxEditBox->SetDisplayMode(theMode);
        this->mAttributeMaxLabel->SetDisplayMode(theMode);
        this->mPercentLabel->SetDisplayMode(theMode);
        this->mAttributePercentCheckBox->SetDisplayMode(theMode);
        break;
    case kSETSObjectVisibleByCheck:
        this->mPlayerBC->SetDisplayMode(theMode);
        break;
    case kSETSObjectHasStateCheck:
        this->mActionComboBox->SetDisplayMode(theMode);
        break;
    case kSETSObjectSelectedByCheck:
        this->mPlayerCC->SetDisplayMode(theMode);
        break;
    }
}

// FUNCTION: EXE 0x592f9f
// Shows the mode radio buttons and the controls of the current mode
void EESETSObject::Show()
{
    this->mUnitsRadio->SetDisplayMode(kDisplayModeShown);
    this->mTypeRadio->SetDisplayMode(kDisplayModeShown);
    this->Refresh();
}

// FUNCTION: EXE 0x592fbf
// Starts a unit selection on a mouse press in the game view while units are picked on the map
void EESETSObject::MouseMove(const GE2DPoint& inLocation)
{
    if (gViewport.GetViewRectangle().Contains(inLocation) && this->mUnitsRadio->GetIsChecked() &&
        (UIManager::GetLMouseButton()->JustPressed() || UIManager::GetRMouseButton()->JustPressed()))
        this->mSelectUnits->StartSelection(inLocation);
}

// FUNCTION: EXE 0x593014
// Finds a unit action name by id (map<unsigned long, UString>::find)

// FUNCTION: EXE 0x59304e
// Returns the first action name entry whose id is not below a key (map<unsigned long, UString>::_Lbound)

// FUNCTION: EXE 0x593077
// Sorts the class list by name, class 0 (any class) first (std::sort with EESETSOUnitTypeNameLess)
// (the sort's helpers follow: 0x593077..0x59370e)

// FUNCTION: EXE 0x593134
// Insertion-sorts a range of the class list by name (std::_Insertion_sort, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59315d
// Quicksorts a range of the class list by name, heap-sorting when too deep (std::_Sort, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593230
// Insertion-sorts the class list unguarded (std::_Unguarded_insertion_sort, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593261
// Insertion-sorts a range of the class list by name (std::_Insertion_sort_1, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59330c
// Compares two unit classes by name, class 0 always first (EESETSOUnitTypeNameLess; also true for 0 vs 0)

// FUNCTION: EXE 0x5933a0
// Makes a heap of a range of the class list (std::make_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5933da
// Sorts a heap of the class list (std::sort_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59341b
// Partitions a range of the class list around a median (std::_Unguarded_partition, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59352c
// Inserts one class into the sorted part before it (std::_Unguarded_insert, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59355c
// Makes a heap of a range of the class list (std::_Make_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5935a0
// Moves the heap's top class to the end (std::pop_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5935c9
// Sifts a class down the heap (std::_Adjust_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593636
// Moves the heap's top class to the end (std::_Pop_heap_0, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593689
// Sifts a class up the heap (std::_Push_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5936da
// Moves the heap's top class to a given slot and re-heaps (std::_Pop_heap, EESETSOUnitTypeNameLess)

// SYNTHETIC: EXE 0x59370e
// (UITriStateCheckBox's scalar deleting destructor, emitted with its vtable)

// SYNTHETIC: EXE 0x59372b
// (EESEEObjectDetail's scalar deleting destructor)

// SYNTHETIC: EXE 0x593756
// (EESEEObjectCheck's scalar deleting destructor)

// SYNTHETIC: EXE 0x593781
// (EESEEObjectUnitSelect's scalar deleting destructor)

// FUNCTION: EXE 0x5937ac
// Destroys a list of strings (vector<UWideString>::~vector, a jump to _Tidy; the ctor calls it)

// FUNCTION: EXE 0x5937b1
// Copies a list of strings (vector<UWideString>::operator=)

// FUNCTION: EXE 0x593925
// Returns the number of strings in a vector<UWideString> (size(), called by operator=)
