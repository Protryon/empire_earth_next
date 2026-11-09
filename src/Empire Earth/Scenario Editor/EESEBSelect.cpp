// C:\EEX\Empire Earth\Scenario Editor\EESEBSelect.cpp (file name inferred: no __FILE__; RTTI EESEBSelect, EESETab). The
// player screen's helper controls: 0x172d2c..0x173218 (the two class-static TMPoll `$E` chains, EESEBSelect's ctor,
// ??_G and slots, Select, EESETab's ctor, ??_G and Check). After EESEPSSetPlayers.cpp (0x172b0f), before
// EESERBUnitTypes.cpp (0x173218: EESEPlayerScreen's own code). Unit flags: the exe module set.
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
#include "ee/EEStringIDs.h"
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
#include "lle/TMPoll.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESEATechTree.h"
#include "ee/EESEBSelect.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// SYNTHETIC: EXE 0x572d2c
// SYNTHETIC: EXE 0x572d36
// SYNTHETIC: EXE 0x572d4b
// SYNTHETIC: EXE 0x572d57
// SYNTHETIC: EXE 0x572d68
// SYNTHETIC: EXE 0x572d72
// SYNTHETIC: EXE 0x572d87
// SYNTHETIC: EXE 0x572d93
// the class statics' `$E` chains 0x172d2c / 0x172d68 (definition order; one guard byte 0x531f50, bits 1 and 2: TMPoll's
// dtor is empty, so each atexit entry only sets its bit)
TMPoll EESEBSelect::sRepeatDelay(500, 0, false);
TMPoll EESEBSelect::sRepeatRate(200, 0, false);

// FUNCTION: EXE 0x572da4
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

// FUNCTION: EXE 0x572e64
// Destroys the arrow button and frees it when asked (EESEBSelect's scalar deleting destructor)

// FUNCTION: EXE 0x572e87
// Moves the selected technology while < or > is held: immediately, again after 500 ms, then every 200 ms
void EESEBSelect::ButtonDown()
{
    if (this->mId >= kSERBUnitTypesAdd && this->mId <= kSERBUnitTypesRemove) {
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

// FUNCTION: EXE 0x572f0e
// Moves every technology across when the << or >> button is released and stops the key repeat
void EESEBSelect::ButtonUp()
{
    if (this->mId >= kSERBUnitTypesAddAll && this->mId <= kSERBUnitTypesRemoveAll)
        this->Select();
    EESEBSelect::sRepeatRate.Stop();
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x572f3b
// Posts a tech tree action that moves the selected technology (or all of them) between the two lists
// for the player chosen in the editor's player combo box
void EESEBSelect::Select()
{
    unsigned long theFirst = 0;
    unsigned long theLast;
    bool theLeft;

    switch (this->mId) {
    case kSERBUnitTypesRemoveAll:
        theLast = ((UIListBox<UWideString>*)this->mForm->GetControlById(kSERBUnitTypesOtherTechList))->GetLineCount();
        if (theLast <= 0)
            return;
        theFirst = 0;
        theLast--;
        theLeft = false;
        break;
    case kSERBUnitTypesAddAll:
        theLast = ((UIListBox<UWideString>*)this->mForm->GetControlById(kSERBUnitTypesTechList))->GetLineCount();
        if (theLast <= 0)
            return;
        theFirst = 0;
        theLast--;
        theLeft = true;
        break;
    case kSERBUnitTypesRemove:
        if (((UIListBox<UWideString>*)this->mForm->GetControlById(kSERBUnitTypesOtherTechList))->GetCurrentSelection(theFirst) != 0)
            return;
        theLast = theFirst;
        theLeft = false;
        break;
    case kSERBUnitTypesAdd:
        if (((UIListBox<UWideString>*)this->mForm->GetControlById(kSERBUnitTypesTechList))->GetCurrentSelection(theFirst) != 0)
            return;
        theLast = theFirst;
        theLeft = true;
        break;
    default:
        return;
    }
    unsigned long thePlayer;
    gUIManager->mScenarioEditorForm->mPlayerComboBox->GetCurrentSelection(thePlayer);
    (new EESEATechTree(theFirst, theLast, thePlayer + 1, theLeft))->Dereference();
}

// FUNCTION: EXE 0x573060
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

// FUNCTION: EXE 0x57318d
// Destroys the tab and frees it when asked (EESETab's scalar deleting destructor)

// FUNCTION: EXE 0x5731b0
// Switches the player screen to the page of the checked tab
void EESETab::Check()
{
    const EEUIFScenarioEditor* theEditor = static_cast<const EEUIFScenarioEditor*>(this->mForm);
    switch (this->mId) {
    case 0x170:
        static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->SelectPage(kPlayerScreenAISettings);
        break;
    case 0x15d:
        static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->SelectPage(kPlayerScreenPlayerPage2);
        break;
    case 0x15a:
        static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->SelectPage(kPlayerScreenOptions);
        break;
    case 0x12d:
        static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->SelectPage(kPlayerScreenTechTree);
        break;
    case 0x12c:
        static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->SelectPage(kPlayerScreenPlayerPage1);
        break;
    }
    UIRadioButton::Check();
}
