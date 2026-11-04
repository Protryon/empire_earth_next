// C:\EEX\Empire Earth\Scenario Editor\EESETriggerScreen.cpp (file name inferred: no __FILE__; RTTI class
// EESETriggerScreen). The editor's trigger tab: 0x17b0b7..0x17ccf3 (ctor, the ??_G pair, dtor, the event handlers and
// slots in source order, the sub-screen factory, then the COMDATs this unit is the first user of: the list and radio
// button ??_G, EEScenarioManager::GetCondition, the events' ??_G, EESEEPanelEvent::SetRectangles). After the "?" unit
// 0x17acf6..0x17b0b7 (a globals chain), before EESETSArea.cpp (0x17ccf3). Unit flags: the exe module set.
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
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EESETSCondition.h"
#include "ee/EESETSEffect.h"
#include "ee/EESETSObject.h"
#include "ee/EESETSArea.h"
#include "ee/EESETSTrigger.h"
#include "ee/EEStringIDs.h"
extern char gU4E5B98[]; // 0x4e5b98: the shared empty string

bool gB4C3220 = true;

// NEAR: inline budget/x87 (the original calls the imported UIEvent ctor for the first events), frame slots of i/theX/
// theY, conditional-temp flags of the check box. GEBitmap::GetHeight must be inline in the exe view. No GE2DPoint
// temporaries: one function-scope point set per control.
// FUNCTION: EXE 0x57b0b7
// Builds the trigger tab: entry list, five sub-tab buttons, new/duplicate/delete, sort and priority buttons,
// debug-id check box, name and description boxes, and the five sub-screens
EESETriggerScreen::EESETriggerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    UWideString theText;
    UIEvent* theEvent;
    GEText* theGEText;

    float theX;
    unsigned long i;
    float theY;
    GE2DPoint thePosition;
    this->mR18 = GERectangle(0.0f, 448.0f, 1023.0f, 606.0f);
    thePosition.SetXY(4.0f, 480.0f);
    this->mList = new UILBString(0x7da, std::vector<UWideString>(), gU531930, false, GERectangle(thePosition, 300.0f, 105.0f),
                           inForm, ioIndex, 3);
    theEvent = new EESEESelectEntry;
    this->mList->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    this->mList->SetBgOpacity(0.4f);
    this->mList->SetScrollBarBgOpacity(0.4f);
    theGEText = this->mList->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mList->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mList, 14);
    ioIndex++;
    for (i = 0; i < 5; i++) {
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_FIRST + i, theText);
        theX = (float)i * 60.0f + 4.0f;
        this->mTabs[i] = new UIRadioButton(2000 + i, theText, gU531930, GERectangle(theX, 458.0f, theX + 57.0f, 477.0f),
                                     inForm, ioIndex, 2, true, i == 0, false);
        this->mTabs[i]->SetUpCheckedBitmap(NULL);
        this->mTabs[i]->SetDownCheckedBitmap(NULL);
        this->mTabs[i]->SetDownUncheckedBitmap(NULL);
        this->mTabs[i]->SetUpUncheckedBitmap(NULL);
        this->mTabs[i]->GetTextLabel()->SetRectangle(GERectangle(theX + 2.0f, 460.0f, theX + 54.0f, 474.0f));
        this->mTabs[i]->SetBgOpacity(0.4f);
        theEvent = new EESEEShowTab;
        this->mTabs[i]->SetTriggerEvent(kControlTriggerChecked, theEvent);
        theEvent->Dereference();
        theGEText = this->mTabs[i]->GetTextLabel()->GetGEText();
        theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mTabs[i], 14);
        ioIndex++;
    }
    gResource->GetString(IDS_TRIGGER_EDITOR_NEW, theText);
    thePosition.SetXY(4.0f, 586.0f);
    this->mNewButton = new UIButton(0x7d5, NULL, NULL, NULL, GERectangle(thePosition, 90.0f, 20.0f), inForm, ioIndex, 2);
    theEvent = new EESEENewEntry;
    this->mNewButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    this->mNewButton->SetBgOpacity(0.4f);
    this->mNewButton->SetText(theText, inFont);
    theGEText = this->mNewButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mNewButton, 14);
    ioIndex++;
    gResource->GetString(IDS_DUPLICATE, theText);
    thePosition.SetXY(104.0f, 586.0f);
    this->mDuplicateButton = new UIButton(kSETriggerScreenDuplicateButton, NULL, NULL, NULL, GERectangle(thePosition, 90.0f, 20.0f), inForm, ioIndex,
                             2);
    theEvent = new EESEEDuplicateEntry;
    this->mDuplicateButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    this->mDuplicateButton->SetBgOpacity(0.4f);
    this->mDuplicateButton->SetText(theText, inFont);
    theGEText = this->mDuplicateButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mDuplicateButton, 14);
    ioIndex++;
    gResource->GetString(IDS_TRIGGER_EDITOR_DELETE, theText);
    thePosition.SetXY(204.0f, 586.0f);
    this->mDeleteButton = new UIButton(kSETriggerScreenDeleteButton, NULL, NULL, NULL, GERectangle(thePosition, 90.0f, 20.0f), inForm, ioIndex,
                             2);
    theEvent = new EESEEDeleteEntry;
    this->mDeleteButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    this->mDeleteButton->SetBgOpacity(0.4f);
    this->mDeleteButton->SetText(theText, inFont);
    theGEText = this->mDeleteButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mDeleteButton, 14);
    ioIndex++;
    thePosition.SetXY(305.0f, 480.0f);
    this->mAlphaSortButton = new UIButton(0x7dd, inForm->GetManager()->GetAlphaSortUpBitmap(), NULL,
                             inForm->GetManager()->GetAlphaSortDownBitmap(),
                             GERectangle(thePosition, inForm->GetManager()->GetAlphaSortUpBitmap()->GetWidth(),
                                         inForm->GetManager()->GetAlphaSortUpBitmap()->GetHeight()),
                             inForm, ioIndex, 0);
    theEvent = new EESEEAlphaSort;
    this->mAlphaSortButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mAlphaSortButton, 14);
    ioIndex++;
    theY = this->mAlphaSortButton->GetRectangle().GetBottom() + 1.0f;
    thePosition.SetXY(305.0f, theY);
    this->mIncPriorityButton = new UIButton(0x7d8, inForm->GetManager()->GetSBBitmapUpNormal(), NULL,
                             inForm->GetManager()->GetSBBitmapUpPressed(),
                             GERectangle(thePosition, inForm->GetManager()->GetSBBitmapUpNormal()->GetWidth(),
                                         inForm->GetManager()->GetSBBitmapUpNormal()->GetHeight()),
                             inForm, ioIndex, 0);
    theEvent = new EESEEIncPriority;
    this->mIncPriorityButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mIncPriorityButton, 14);
    ioIndex++;
    theY = this->mIncPriorityButton->GetRectangle().GetBottom() + 1.0f;
    thePosition.SetXY(305.0f, theY);
    this->mDecPriorityButton = new UIButton(0x7d9, inForm->GetManager()->GetSBBitmapDownNormal(), NULL,
                             inForm->GetManager()->GetSBBitmapDownPressed(),
                             GERectangle(thePosition, inForm->GetManager()->GetSBBitmapDownNormal()->GetWidth(),
                                         inForm->GetManager()->GetSBBitmapDownNormal()->GetHeight()),
                             inForm, ioIndex, 0);
    theEvent = new EESEEDecPriority;
    this->mDecPriorityButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mDecPriorityButton, 14);
    ioIndex++;
    theY = this->mDecPriorityButton->GetRectangle().GetBottom() + 1.0f;
    thePosition.SetXY(305.0f, theY);
    this->mShowIDsCheckBox = new EESETSShowIDsCheckBox(0x7de, gU4E5B98, gU531930,
                                            thePosition,
                                            inForm->GetManager()->GetCheckOn()->GetWidth(),
                                            inForm->GetManager()->GetCheckOn()->GetHeight(),
                                            inForm, ioIndex);
    theEvent = new EESEEShowDebugIDs;
    this->mShowIDsCheckBox->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mShowIDsCheckBox, 14);
    ioIndex++;
    gResource->GetString(IDS_TRIGGER_EDITOR_NAME, theText);
    thePosition.SetXY(329.0f, 458.0f);
    this->mNameLabel = new UITextLabel(theText, inFont, kSETriggerScreenNameLabel, GERectangle(thePosition, 250.0f,
                                                                     (float)inFont->GetHeight()),
                                 inForm, 0, ioIndex, 0);
    this->mNameLabel->SetBgOpacity(0.4f);
    theGEText = this->mNameLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mNameLabel, 14);
    ioIndex++;
    thePosition.SetXY(329.0f, 480.0f);
    this->mNameEditBox = new UIEditBox(kSETriggerScreenNameEditBox, gU531930, 0x1c, false,
                                 GERectangle(thePosition, 250.0f, (float)gU531930->GetHeight() + 6.0f), inForm,
                                 ioIndex, 2, false, false);
    this->mNameEditBox->SetBgOpacity(0.4f);
    theGEText = this->mNameEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    theEvent = new EESEESelectName;
    this->mNameEditBox->SetTriggerEvent(kControlTriggerEnterPressed, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mNameEditBox, 14);
    ioIndex++;
    gResource->GetString(IDS_DESCRIPTION_76A8, theText);
    thePosition.SetXY(589.0f, 458.0f);
    this->mTextLabel = new UITextLabel(theText, inFont, 0x962, GERectangle(thePosition, 220.0f,
                                                                     (float)inFont->GetHeight()),
                                 inForm, 0, ioIndex, 0);
    this->mTextLabel->SetBgOpacity(0.4f);
    theGEText = this->mTextLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mTextLabel->SetDisplayMode(kDisplayModeHidden);
    inForm->AddControl(this->mTextLabel, 14);
    ioIndex++;
    theY = this->mTextLabel->GetRectangle().GetBottom() + 5.0f;
    thePosition.SetXY(589.0f, theY);
    this->mTextEditBox = new UIEditBox(0x963, gU531930, 6, false, GERectangle(thePosition, 220.0f, 98.0f), inForm,
                                 ioIndex, 2, true, false);
    this->mTextEditBox->SetBgOpacity(0.4f);
    theGEText = this->mTextEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    inForm->AddControl(this->mTextEditBox, 14);
    ioIndex++;
    for (i = 0; i < 5; i++)
        this->mScreens[i] = this->CreateScreen(i, inFont, inForm, ioIndex);
    this->HideGroups();
    this->mCurrentScreen = kSETriggerScreenTrigger;
    this->mCurrentEntityGroup = kEntityGroupTrigger;
}

// SYNTHETIC: EXE 0x57bf58
// (EESETSShowIDsCheckBox's scalar deleting destructor)

// SYNTHETIC: EXE 0x57bf7b
// (EESETriggerScreen's scalar deleting destructor)

// FUNCTION: EXE 0x57bf97
// Destroys the trigger tab's list, tab buttons, sub-screens, name and description controls and buttons
EESETriggerScreen::~EESETriggerScreen()
{
    unsigned long i;
    delete this->mList;
    for (i = 0; i < 5; i++)
        delete this->mTabs[i];
    for (i = 0; i < 5; i++)
        delete this->mScreens[i];
    delete this->mNameEditBox;
    delete this->mNameLabel;
    delete this->mTextLabel;      // the description label and edit box were never deleted (leak)
    delete this->mTextEditBox;
    delete this->mNewButton;
    delete this->mDuplicateButton;
    delete this->mDeleteButton;
    delete this->mIncPriorityButton;
    delete this->mDecPriorityButton;
    delete this->mAlphaSortButton;
    delete this->mShowIDsCheckBox;
}

// FUNCTION: EXE 0x57c057
// Sorts the current sub-screen's entries alphabetically and selects the first one (only with a selection)
void EESETriggerScreen::AlphaSort()
{
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) != kErrorOK)
        return;
    this->StoreEntry();
    this->mScreens[this->mCurrentScreen]->Sort();
    this->mScreens[this->mCurrentScreen]->mSelection = 0;
    this->FillList();
    this->mList->ChangeSelectionIndex(0, false, true);
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c0ad
// Stores the open entry and loads the list's selected entry (the last one when nothing is selected)
void EESETriggerScreen::SelectEntry()
{
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) == kErrorOK) {
        this->StoreEntry();
        this->mScreens[this->mCurrentScreen]->mSelection = theSelection;
        this->FillList();
        this->mList->ChangeSelectionIndex(theSelection, false, false);
        this->ShowSelection();
    } else if (this->mList->GetLineCount() > 0) {
        theSelection = this->mList->GetLineCount() - 1;
        this->StoreEntry();
        this->mScreens[this->mCurrentScreen]->mSelection = theSelection;
        this->FillList();
        this->mList->ChangeSelectionIndex(theSelection, false, false);
        this->ShowSelection();
    } else {
        this->mScreens[this->mCurrentScreen]->mSelection = 0;
        this->mScreens[this->mCurrentScreen]->mHasSelection = false;
    }
}

// FUNCTION: EXE 0x57c130
// Leaves the trigger tab: hides the hide-UI check box, restores the full game view, stores the open entry
// and deactivates the sub-screen
void EESETriggerScreen::Leave()
{
    GERectangle theRectangle(0.0f, 0.0f, 1024.0f, 606.0f);
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mNewButton->GetForm());
    theEditor->GetControlById(kScenarioEditorHideUICheckBox)->SetDisplayMode(kDisplayModeHidden);
    theEditor->SetGameView(theRectangle, false);
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (gMiniMap != NULL)
        gMiniMap->RefreshView();
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
    this->HideGroups();
    this->StoreEntry();
    this->mScreens[this->mCurrentScreen]->Deactivate();
}

// FUNCTION: EXE 0x57c1d9
// Updates the current sub-screen
void EESETriggerScreen::Update()
{
    this->mScreens[this->mCurrentScreen]->Update();
}

// FUNCTION: EXE 0x57c1e5
// Creates the trigger, condition, effect, object or area sub-screen for a sub-tab index
EESETSBaseObject* EESETriggerScreen::CreateScreen(unsigned long inType, GEFont* inFont, UIForm* inForm,
                                                  unsigned long& ioIndex)
{
    EESETSBaseObject* theScreen = NULL;
    switch (inType) {
    case kSETriggerScreenTrigger:
        theScreen = new EESETSTrigger(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenCondition:
        theScreen = new EESETSCondition(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenEffect:
        theScreen = new EESETSEffect(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenObject:
        theScreen = new EESETSObject(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenArea:
        theScreen = new EESETSArea(inFont, inForm, ioIndex);
        break;
    }
    return theScreen;
}

// FUNCTION: EXE 0x57c2ae
// Moves the selected entry one place down the list and keeps it selected
void EESETriggerScreen::DecPriority()
{
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) != kErrorOK || !(theSelection < this->mList->GetLineCount() - 1))
        return;
    this->StoreEntry();
    this->mScreens[this->mCurrentScreen]->MoveDown();
    this->mScreens[this->mCurrentScreen]->mSelection = theSelection + 1;
    this->FillList();
    this->mList->ChangeSelectionIndex(theSelection + 1, false, true);
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c31b
// Deletes the selected entry and selects the entry now in its place (or the last one)
void EESETriggerScreen::DeleteEntry()
{
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) != kErrorOK)
        return;
    this->mScreens[this->mCurrentScreen]->Delete();
    this->FillList();
    this->mScreens[this->mCurrentScreen]->mHasSelection = false;
    if (theSelection < this->mList->GetLineCount())
        this->mList->ChangeSelectionIndex(theSelection, false, true);
    else
        this->mList->ChangeSelectionIndex(this->mList->GetLineCount() - 1, false, true);
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c399
// Draws the panel background behind the trigger tab unless the editor UI is hidden
void EESETriggerScreen::Draw(const GERectangle& inRectangle)
{
    GERectangle theClip;
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    if (!theEditor->mWasUIHidden && inRectangle.And(this->mR18, theClip))
        theEditor->UIForm::DrawBackground(theEditor->mPanelBackground, theClip, GERectangle(0.0f, 0.0f, 256.0f, 256.0f), this->mR18);
}

// FUNCTION: EXE 0x57c411
// Duplicates the selected entry and selects the copy at the end of the list
void EESETriggerScreen::DuplicateEntry()
{
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) != kErrorOK)
        return;
    this->StoreEntry();
    this->mScreens[this->mCurrentScreen]->Duplicate();
    this->FillList();
    this->mList->ChangeSelectionIndex(this->mList->GetLineCount() - 1, false, true);
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c467
// Switches to a tab button's sub-screen: stores the open entry, refills the list and reselects the
// sub-screen's entry (the last one when it is out of range)
void EESETriggerScreen::ShowTab(unsigned long inControlID)
{
    UIForm* theForm = this->mNewButton->GetForm();
    this->StoreEntry();
    theForm->GetEntityGroup(this->mCurrentEntityGroup)->Disable();
    this->GetScreen(this->mCurrentScreen)->Deactivate();
    this->HideText();
    switch (inControlID) {
    case kSETriggerTabTrigger:
        this->mCurrentEntityGroup = kEntityGroupTrigger;
        this->mCurrentScreen = kSETriggerScreenTrigger;
        break;
    case kSETriggerTabCondition:
        this->mCurrentEntityGroup = kEntityGroupCondition;
        this->mCurrentScreen = kSETriggerScreenCondition;
        break;
    case kSETriggerTabEffect:
        this->mCurrentEntityGroup = kEntityGroupEffect;
        this->mCurrentScreen = kSETriggerScreenEffect;
        break;
    case kSETriggerTabObject:
        this->mCurrentEntityGroup = kEntityGroupObject;
        this->mCurrentScreen = kSETriggerScreenObject;
        break;
    case kSETriggerTabArea:
        this->mCurrentEntityGroup = kEntityGroupArea;
        this->mCurrentScreen = kSETriggerScreenArea;
        break;
    }
    this->FillList();
    unsigned long theSelection;
    if (this->GetScreen(this->mCurrentScreen)->mHasSelection && this->mList->GetLineCount() > (theSelection = this->GetScreen(this->mCurrentScreen)->mSelection))
        this->mList->ChangeSelectionIndex(this->GetScreen(this->mCurrentScreen)->mSelection, false, true);
    else if (this->mList->GetLineCount() > 0) {
        unsigned long theCount = this->mList->GetLineCount();
        this->GetScreen(this->mCurrentScreen)->mSelection = theCount - 1;
        this->mList->ChangeSelectionIndex(this->GetScreen(this->mCurrentScreen)->mSelection, false, true);
    } else {
        this->GetScreen(this->mCurrentScreen)->mHasSelection = false;
        this->GetScreen(this->mCurrentScreen)->mSelection = 0;
    }
    this->GetScreen(this->mCurrentScreen)->Activate();
    this->ShowSelection();
    gUIManager->RedrawAll();
}

// FUNCTION: EXE 0x57c599
// Writes the controls back into the open entry, if there is one (sets gB4C3220, which ShowSelection clears)
void EESETriggerScreen::StoreEntry()
{
    gB4C3220 = true;
    if (this->mScreens[this->mCurrentScreen]->mHasSelection)
        this->mScreens[this->mCurrentScreen]->Store();
}

// FUNCTION: EXE 0x57c5b3
// Disables the trigger tab's control group and hides the current sub-screen's controls
void EESETriggerScreen::Disable()
{
    this->mNameEditBox->GetForm()->GetEntityGroup(14)->Disable();
    this->mScreens[this->mCurrentScreen]->Hide();
}

// FUNCTION: EXE 0x57c5d9
// Hides the description label and edit box
void EESETriggerScreen::HideText()
{
    this->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    this->mTextLabel->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57c5f2
// Hides the alphabetical sort button
void EESETriggerScreen::HideAlphaSort()
{
    this->mAlphaSortButton->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57c5fd
// Loads the list's selected entry into the sub-screen and shows the name, duplicate and delete controls;
// without a selection hides them and the description
void EESETriggerScreen::ShowSelection()
{
    unsigned long theSelection;
    gB4C3220 = false;
    this->mShowIDsCheckBox->SetChecked(gWorld.mScenarioManager->mShowDebugIDs);
    if (this->mList->GetCurrentSelection(theSelection) == kErrorOK) {
        this->mScreens[this->mCurrentScreen]->mSelection = theSelection;
        this->mScreens[this->mCurrentScreen]->mHasSelection = true;
        this->mNameEditBox->GetForm()->GetEntityGroup(this->mCurrentEntityGroup)->Enable();
        this->mNameEditBox->SetDisplayMode(kDisplayModeShown);
        this->mNameLabel->SetDisplayMode(kDisplayModeShown);
        this->mDeleteButton->SetDisplayMode(kDisplayModeShown);
        this->mDuplicateButton->SetDisplayMode(kDisplayModeShown);
        this->mScreens[this->mCurrentScreen]->Load();
    } else {
        this->mScreens[this->mCurrentScreen]->mHasSelection = false;
        this->mNameEditBox->GetForm()->GetEntityGroup(this->mCurrentEntityGroup)->Disable();
        this->mNameEditBox->SetDisplayMode(kDisplayModeHidden);
        this->mNameLabel->SetDisplayMode(kDisplayModeHidden);
        this->mDeleteButton->SetDisplayMode(kDisplayModeHidden);
        this->mDuplicateButton->SetDisplayMode(kDisplayModeHidden);
        this->HideText();
    }
}

// FUNCTION: EXE 0x57c6e0
// Moves the selected entry one place up the list and keeps it selected
void EESETriggerScreen::IncPriority()
{
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) != kErrorOK || theSelection <= 0)
        return;
    this->StoreEntry();
    this->mScreens[this->mCurrentScreen]->MoveUp();
    this->mScreens[this->mCurrentScreen]->mSelection = theSelection - 1;
    this->FillList();
    this->mList->ChangeSelectionIndex(theSelection - 1, false, true);
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c743
// Disables the five sub-screens' control groups (9..13)
void EESETriggerScreen::HideGroups()
{
    UIForm* theForm = this->mNewButton->GetForm();
    theForm->GetEntityGroup(9)->Disable();
    theForm->GetEntityGroup(10)->Disable();
    theForm->GetEntityGroup(11)->Disable();
    theForm->GetEntityGroup(12)->Disable();
    theForm->GetEntityGroup(13)->Disable();
}

// FUNCTION: EXE 0x57c78b
// Loads the list's selected entry into the controls
void EESETriggerScreen::Load()
{
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c790
// Stores the open entry, adds a new entry to the current sub-screen and selects it
void EESETriggerScreen::NewEntry()
{
    this->StoreEntry();
    this->mScreens[this->mCurrentScreen]->New();
    this->FillList();
    this->mList->ChangeSelectionIndex(this->mList->GetLineCount() - 1, false, true);
    this->ShowSelection();
}

// FUNCTION: EXE 0x57c7d0
// Enters the trigger tab: shrinks the game view above the panel, shows the hide-UI check box, refills the
// list, reselects the sub-screen's entry if still valid and activates the sub-screen
void EESETriggerScreen::Enter()
{
    GERectangle theRectangle(0.0f, 0.0f, 1024.0f, 447.0f);
    GERectangle theEdges(0.0f, 0.0f, 1024.0f, 606.0f);
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mNewButton->GetForm());
    EESetGameViewRectangle(theRectangle);
    theEditor->GetControlById(kScenarioEditorHideUICheckBox)->SetDisplayMode(kDisplayModeShown);
    static_cast<EESEEPanelEvent*>(theEditor->GetControlById(kScenarioEditorHideUICheckBox)->GetTriggerEvent(kControlTriggerChecked))->SetRectangles(theEdges, theRectangle);
    theEditor->SetGameView(theRectangle, false);
    static_cast<UICheckBox*>(theEditor->GetControlById(kScenarioEditorHideUICheckBox))->SetChecked(true);
    theEditor->GetEntityGroupById(14)->Enable();
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (gMiniMap != NULL)
        gMiniMap->RefreshView();
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
    theEditor->GetEntityGroup(this->mCurrentEntityGroup)->Enable();
    this->FillList();
    if (this->GetScreen(this->mCurrentScreen)->mHasSelection) {
        unsigned long theSelection = this->GetScreen(this->mCurrentScreen)->mSelection;
        if (theSelection < this->mList->GetLineCount())
            this->mList->ChangeSelectionIndex(this->GetScreen(this->mCurrentScreen)->mSelection, false, true);
        else
            this->GetScreen(this->mCurrentScreen)->mHasSelection = false;
    }
    this->GetScreen(this->mCurrentScreen)->Activate();
}

// FUNCTION: EXE 0x57c92e
// Stores the entry after Enter in its name box, refreshes the list and puts the caret at the end of the name
void EESETriggerScreen::SelectName()
{
    UWideString theText;
    unsigned long theSelection;
    if (this->mList->GetCurrentSelection(theSelection) != kErrorOK)
        return;
    this->StoreEntry();
    this->ShowSelection();
    this->FillList();
    this->mList->ChangeSelectionIndex(theSelection, false, false);
    this->mNameEditBox->GetAllText(theText);
    this->mNameEditBox->SetCaretLocation(theText.GetLength(), 0);
}

// FUNCTION: EXE 0x57c99f
// Refills the entry list from the current sub-screen; its control group is enabled only when there are entries
void EESETriggerScreen::FillList()
{
    std::vector<UWideString> theNames;
    this->mList->DeleteAllEntries();
    this->mScreens[this->mCurrentScreen]->mHasSelection = false;
    this->mScreens[this->mCurrentScreen]->GetNames(theNames);
    this->mList->AppendStringArray(theNames);
    if (this->mList->GetLineCount() > 0) {
        this->mList->GetForm()->GetEntityGroup(this->mCurrentEntityGroup)->Enable();
        this->mScreens[this->mCurrentScreen]->mHasSelection = true;
    } else
        this->mList->GetForm()->GetEntityGroup(this->mCurrentEntityGroup)->Disable();
}

// FUNCTION: EXE 0x57ca30
// Enables the trigger tab's control group and shows the current sub-screen's controls
void EESETriggerScreen::Enable()
{
    this->mNameEditBox->GetForm()->GetEntityGroup(14)->Enable();
    this->mScreens[this->mCurrentScreen]->Show();
}

// FUNCTION: EXE 0x57ca56
// Shows the alphabetical sort button
void EESETriggerScreen::ShowAlphaSort()
{
    this->mAlphaSortButton->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x57ca61
// Writes the controls back into the open entry
void EESETriggerScreen::Store()
{
    this->StoreEntry();
}

// FUNCTION: EXE 0x57ca66
// Passes keyboard handling to the current sub-screen when it has a selection
void EESETriggerScreen::UpdateKeys()
{
    if (this->mScreens[this->mCurrentScreen]->mHasSelection)
        this->mScreens[this->mCurrentScreen]->UpdateKeys();
}

// FUNCTION: EXE 0x57ca79
// Passes mouse movement to the current sub-screen when it has a selection
void EESETriggerScreen::MouseMove(const GE2DPoint& inLocation)
{
    if (this->mScreens[this->mCurrentScreen]->mHasSelection)
        this->mScreens[this->mCurrentScreen]->MouseMove(inLocation);
}

// FUNCTION: EXE 0x57ca92
// Refreshes the current sub-screen's controls when it has a selection
void EESETriggerScreen::RefreshScreen()
{
    if (this->mScreens[this->mCurrentScreen]->mHasSelection)
        this->mScreens[this->mCurrentScreen]->RefreshControls();
}

// SYNTHETIC: EXE 0x57caa5
// (UILBString's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cac2
// (UIRadioButton's scalar deleting destructor)

// MISSING 0x57cadf: EEScenarioManager::GetCondition's COMDAT (inline, ee/EEScenarioManager.h); no user written in this
// unit yet, so it is not emitted here

// SYNTHETIC: EXE 0x57cb0c
// (EESEEShowTab's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cb37
// (EESEESelectEntry's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cb62
// (EESEENewEntry's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cb8d
// (EESEEDuplicateEntry's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cbb8
// (EESEEDeleteEntry's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cbe3
// (EESEEIncPriority's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cc0e
// (EESEEDecPriority's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cc39
// (EESEEAlphaSort's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cc64
// (EESEEShowDebugIDs's scalar deleting destructor)

// SYNTHETIC: EXE 0x57cc8f
// (EESEESelectName's scalar deleting destructor)

// FUNCTION: EXE 0x57ccba
// Sets the hide-UI panel event's full and reduced game view rectangles
// EESEEPanelEvent::SetRectangles (inline, ee/EESETriggerScreen.h)
