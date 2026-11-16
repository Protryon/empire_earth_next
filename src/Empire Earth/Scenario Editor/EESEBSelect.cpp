// C:\EEX\Empire Earth\Scenario Editor\EESEBSelect.cpp (file name inferred: no __FILE__; RTTI EESEBSelect, EESETab). The
// player screen's helper controls: 0x172d2c..0x173218 (the two class-static TMPoll `$E` chains, EESEBSelect's ctor,
// ??_G and slots, Select, EESETab's ctor, ??_G and Check). After EESEPSSetPlayers.cpp (0x172b0f), before
// EESERBUnitTypes.cpp (0x173218: EESEPlayerScreen's own code). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "ee/EEStringIDs.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "lle/USingleton.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESEARust.h"
#include "ee/EESEBSelect.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// the class statics' `$E` chains 0x172d2c / 0x172d68 (definition order; one guard byte 0x531f50, bits 1 and 2: TMPoll's
// dtor is empty, so each atexit entry only sets its bit)
TMPoll EESEBSelect::sRepeatDelay(500, 0, false);
TMPoll EESEBSelect::sRepeatRate(200, 0, false);

// Creates a 40x25 tech tree arrow button labelled <<, >>, < or > by its id, with white transparent text
EESEBSelect::EESEBSelect(unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm, GEFont* inFont,
                         unsigned long inIndex)
    : UIButton(inId, NULL, NULL, NULL, GERectangle(inLocation, 40.0f, 25.0f), inForm, inIndex, 3)
{
    UWideString theText;
    switch (inId) {
    case kSelectButtonMoveAllLeft:
        theText = "<<";
        break;
    case kSelectButtonMoveAllRight:
        theText = ">>";
        break;
    case kSelectButtonMoveLeft:
        theText = "<";
        break;
    case kSelectButtonMoveRight:
        theText = ">";
        break;
    }
    this->SetText(theText, inFont);
    GEText* theGEText = this->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
}

// Moves the selected technology while < or > is held: immediately, again after 500 ms, then every 200 ms
void EESEBSelect::ButtonDown()
{
    if (this->GetId() >= kSERBUnitTypesAdd && this->GetId() <= kSERBUnitTypesRemove) {
        if (EESEBSelect::sRepeatDelay.GetIsRunning()) {
            if (EESEBSelect::sRepeatDelay.Poll() > 0) {
                this->Select();
                EESEBSelect::sRepeatRate.Start();
                EESEBSelect::sRepeatDelay.Stop();
            }
        } else if (EESEBSelect::sRepeatRate.GetIsRunning()) {
            if (EESEBSelect::sRepeatRate.Poll() > 0)
                this->Select();
        } else {
            this->Select();
            EESEBSelect::sRepeatDelay.Start();
        }
    }
    UIButton::ButtonDown();
}

// Moves every technology across when the << or >> button is released and stops the key repeat
void EESEBSelect::ButtonUp()
{
    if (this->GetId() >= kSERBUnitTypesAddAll && this->GetId() <= kSERBUnitTypesRemoveAll)
        this->Select();
    EESEBSelect::sRepeatRate.Stop();
    UIButton::ButtonUp();
}

// Posts a tech tree action that moves the selected technology (or all of them) between the two lists
// for the player chosen in the editor's player combo box
void EESEBSelect::Select()
{
    unsigned long theFirst = 0;
    unsigned long theLast;
    bool theLeft;

    switch (this->GetId()) {
    case kSERBUnitTypesRemoveAll:
        theLast = ((UIListBox<UWideString>*)this->GetForm()->GetControlById(kSERBUnitTypesOtherTechList))->GetLineCount();
        if (theLast <= 0)
            return;
        theFirst = 0;
        theLast--;
        theLeft = false;
        break;
    case kSERBUnitTypesAddAll:
        theLast = ((UIListBox<UWideString>*)this->GetForm()->GetControlById(kSERBUnitTypesTechList))->GetLineCount();
        if (theLast <= 0)
            return;
        theFirst = 0;
        theLast--;
        theLeft = true;
        break;
    case kSERBUnitTypesRemove:
        if (((UIListBox<UWideString>*)this->GetForm()->GetControlById(kSERBUnitTypesOtherTechList))->GetCurrentSelection(theFirst) != 0)
            return;
        theLast = theFirst;
        theLeft = false;
        break;
    case kSERBUnitTypesAdd:
        if (((UIListBox<UWideString>*)this->GetForm()->GetControlById(kSERBUnitTypesTechList))->GetCurrentSelection(theFirst) != 0)
            return;
        theLast = theFirst;
        theLeft = true;
        break;
    default:
        return;
    }
    unsigned long thePlayer;
    gUIManager->mScenarioEditorForm->GetPlayerComboBox()->GetCurrentSelection(thePlayer);
    (new EESEARust(kSEATechTree, NULL, theFirst, theLast, (int32_t)thePlayer + 1, theLeft))->Dereference();
}

// Creates a 150x25 player screen tab (player pages 1/2, options, tech tree, AI settings) without bitmaps
EESETab::EESETab(unsigned long inId, const GE2DPoint& inLocation, GEFont* inFont, UIForm* inForm,
                 unsigned long inIndex, bool inIsChecked)
    : UIRadioButton(inId, UWideString(gEmptyString), inFont, GERectangle(inLocation, 150.0f, 25.0f), inForm, inIndex, 3, true, inIsChecked,
                    false)
{
    UWideString theText;
    switch (inId) {
    case kPlayerTabAISettings:
        gResource->GetString(IDS_AI_SETTINGS, theText);
        break;
    case kPlayerTabPlayerPage2:
        gResource->GetString(IDS_PLAYER_PAGE_2, theText);
        break;
    case kPlayerTabOptions:
        gResource->GetString(IDS_SE_OPTIONS, theText);
        break;
    case kPlayerTabTechTree:
        gResource->GetString(IDS_TECH_TREE, theText);
        break;
    case kPlayerTabPlayerPage1:
        gResource->GetString(IDS_PLAYER_PAGE_1, theText);
        break;
    }
    this->SetDownCheckedBitmap(NULL);
    this->SetUpCheckedBitmap(NULL);
    this->SetDownUncheckedBitmap(NULL);
    this->SetUpUncheckedBitmap(NULL);
    UIButton::SetText(theText, inFont);
    GEText* theGEText = this->GetGEText();
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theGEText->SetIsTransparent(true);
}

// Switches the player screen to the page of the checked tab
void EESETab::Check()
{
    const EEUIFScenarioEditor* theEditor = static_cast<const EEUIFScenarioEditor*>(this->GetForm());
    switch (this->GetId()) {
    case 0x170:
        static_cast<EESEPlayerScreen*>(theEditor->GetTabGroup(kEditorTabPlayers))->SelectPage(kPlayerScreenAISettings);
        break;
    case 0x15d:
        static_cast<EESEPlayerScreen*>(theEditor->GetTabGroup(kEditorTabPlayers))->SelectPage(kPlayerScreenPlayerPage2);
        break;
    case 0x15a:
        static_cast<EESEPlayerScreen*>(theEditor->GetTabGroup(kEditorTabPlayers))->SelectPage(kPlayerScreenOptions);
        break;
    case 0x12d:
        static_cast<EESEPlayerScreen*>(theEditor->GetTabGroup(kEditorTabPlayers))->SelectPage(kPlayerScreenTechTree);
        break;
    case 0x12c:
        static_cast<EESEPlayerScreen*>(theEditor->GetTabGroup(kEditorTabPlayers))->SelectPage(kPlayerScreenPlayerPage1);
        break;
    }
    UIRadioButton::Check();
}
