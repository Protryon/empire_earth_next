// C:\EEX\Empire Earth\Scenario Editor\EESEEvents.cpp (file name inferred: no __FILE__; class prefix EESEE). The
// scenario editor's UI events: 0x16f892..0x16ff5f. The trigger tab's events (0x16f892..0x16fd46: its buttons, the
// object, area, condition, effect and trigger screens' events) are EEScreenEvent codes handled by ee_screens
// scenario_editor.rs over this unit's host, ee_screenscpp_se (docs/RUST_PLAN.md "R6 game screens"); the events with
// per-instance state (the slider's label, the check box's controls, the scenario combo box's refill) stay here.
// The host also serves the editor form's and its tabs' events (the toolbar, the hot keys, the elevation tool, the
// player tab's epoch, the effects' radios, the instructions' "///" expansion: the units that newed them).
// After RTSPlayer.cpp (..0x16f892), before EESEIAExport.cpp's Virtual4 0x16ff5f. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>

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

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEACenterCamera.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETSTrigger.h"
#include "ee/EESETSCondition.h"
#include "ee/EESETSEffect.h"
#include "ee/EESETSObject.h"
#include "ee/EESETSArea.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESEEvents.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "lle/GETransformation.h"
#include "ee/EEUserInterface.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESEUnit.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESEARust.h"
void FillScenarioComboBox(EESETSEScenarioComboBox* inComboBox, UITextLabel* inLabel, bool inAreas);
void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox);   // EESETSETGame.cpp

// ---- the host of the editor's events (ee_screens scenario_editor.rs): inForm is the scenario editor's form,
// inEntity the event's control. The editor's objects, one call each (EE_SCREENS_SE_OP_*).
static GE2DPoint sAreaCenter;   // EE_SCREENS_SE_OP_AREA_CENTER's, for EE_SCREENS_SE_OP_CENTER_ON_AREA

// the exit question's answers (on the question's form)
static void EESESaveBeforeExit(UIForm* inForm) { ee_screens_event(EE_SCREENS_SE_SAVE_BEFORE_EXIT, inForm, NULL); }
static void EESEExitWithoutSaving(UIForm* inForm) { ee_screens_event(EE_SCREENS_SE_EXIT_WITHOUT_SAVING, inForm, NULL); }

// a text (EE_SCREENS_SE_TEXT_*) into outText (up to inCapacity bytes); its length in bytes
extern "C" size_t ee_screenscpp_se_text(void* inForm, void* inEntity, uint32_t inWhich, uint32_t inArg, uint8_t* outText,
                                        size_t inCapacity)
{
    UWideString theText;
    if (inWhich == EE_SCREENS_SE_TEXT_EDIT_LINE)
        theText = static_cast<UIEditBox*>(static_cast<UIEntity*>(inEntity))->GetText(inArg);
    else
        gResource->GetString((int32_t)inArg, theText);
    size_t theLength = theText.GetTCHARLength();
    memcpy(outText, (const char*)theText, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

// the entity's (an edit box's) text
extern "C" void ee_screenscpp_se_set_text(void* inEntity, const uint8_t* inText, size_t inLength)
{
    UWideString theText(std::string(reinterpret_cast<const char*>(inText), inLength).c_str());
    static_cast<UIEditBox*>(static_cast<UIEntity*>(inEntity))->SetText(theText);
}

static void EESEPostAction(EEAction* inAction)
{
    gUserInterface->PostAction(inAction);
    inAction->Dereference();
}

extern "C" uint32_t ee_screenscpp_se(void* inForm, void* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB)
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(inForm));
    UIEntity* theEntity = static_cast<UIEntity*>(inEntity);
    auto Screen = [&](long inIndex) { return theEditor->GetTriggerScreen()->GetScreen(inIndex); };
    auto Object = [&]() { return static_cast<EESETSObject*>(Screen(kSETriggerScreenObject)); };
    auto Trigger = [&]() { return static_cast<EESETSTrigger*>(Screen(kSETriggerScreenTrigger)); };
    switch (inOp) {
    case EE_SCREENS_SE_OP_ENTITY_ID:
        return static_cast<UIControl*>(theEntity)->GetId();
    case EE_SCREENS_SE_OP_TAB: {
        EESETriggerScreen* theScreen = theEditor->GetTriggerScreen();
        switch (inA) {
        case EE_SCREENS_SE_TAB_SHOW: theScreen->ShowTab(inB); break;
        case EE_SCREENS_SE_TAB_SELECT_ENTRY: theScreen->SelectEntry(); break;
        case EE_SCREENS_SE_TAB_NEW_ENTRY: theScreen->NewEntry(); break;
        case EE_SCREENS_SE_TAB_DUPLICATE_ENTRY: theScreen->DuplicateEntry(); break;
        case EE_SCREENS_SE_TAB_DELETE_ENTRY: theScreen->DeleteEntry(); break;
        case EE_SCREENS_SE_TAB_INC_PRIORITY: theScreen->IncPriority(); break;
        case EE_SCREENS_SE_TAB_DEC_PRIORITY: theScreen->DecPriority(); break;
        case EE_SCREENS_SE_TAB_ALPHA_SORT: theScreen->AlphaSort(); break;
        default: theScreen->SelectName(); break;
        }
        break;
    }
    case EE_SCREENS_SE_OP_TOGGLE_DEBUG_IDS:
        gWorld.mScenarioManager->mShowDebugIDs = !gWorld.mScenarioManager->mShowDebugIDs;
        break;
    case EE_SCREENS_SE_OP_OBJECT_SET_MAP:
        Object()->mIsSelectOnMap = inA != 0;
        break;
    case EE_SCREENS_SE_OP_OBJECT_REFRESH:
        Object()->Refresh();
        break;
    case EE_SCREENS_SE_OP_OBJECT_IS_MAP:
        return Object()->GetIsSelectOnMap();
    case EE_SCREENS_SE_OP_MANAGER_CLEAR_SELECTION:
        gUIManager->mScenarioEditorForm->ClearSelection();
        break;
    case EE_SCREENS_SE_OP_OBJECT_CLEAR_UNIT_LIST:
        Object()->ClearUnitList();
        break;
    case EE_SCREENS_SE_OP_FORM_CLEAR_SELECTION:
        theEditor->ClearSelection();
        break;
    case EE_SCREENS_SE_OP_OBJECT_CLEAR_IDS:
        Object()->mUnitIDs.clear();
        break;
    case EE_SCREENS_SE_OP_OBJECT_UPDATE_SELECTION:
        Object()->UpdateSelection();
        break;
    case EE_SCREENS_SE_OP_OBJECT_CHECK_CHANGED:
        Object()->CheckChanged(theEntity);
        break;
    case EE_SCREENS_SE_OP_LIST_SELECTION: {
        unsigned long theSelection;
        return static_cast<UILBString*>(theEntity)->GetCurrentSelection(theSelection) == kErrorOK ? (uint32_t)theSelection
                                                                                                    : 0xffffffffu;
    }
    case EE_SCREENS_SE_OP_SELECTED_UNIT_COUNT:
        return theEditor->GetSelectedUnitCount();
    case EE_SCREENS_SE_OP_CENTER_ON_UNIT: {
        EEACenterCamera* theAction = new EEACenterCamera;
        theAction->SetUnit(theEditor->GetSelectedUnit(inA));
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_SE_OP_COMBO_SELECTION: {
        // (the original read the selection whatever the call answered: none reads as no selection here)
        unsigned long theSelection;
        return static_cast<UIComboBox*>(theEntity)->GetCurrentSelection(theSelection) == kErrorOK ? (uint32_t)theSelection
                                                                                                    : 0xffffffffu;
    }
    case EE_SCREENS_SE_OP_AREA_MODE:
        static_cast<EESETSArea*>(Screen(kSETriggerScreenArea))->SetAreaMode(inA != 0);
        break;
    case EE_SCREENS_SE_OP_AREA_CENTER:
        return static_cast<EESETSArea*>(Screen(kSETriggerScreenArea))->GetCenter(sAreaCenter);
    case EE_SCREENS_SE_OP_CENTER_ON_AREA: {
        EEACenterCamera* theAction = new EEACenterCamera;
        theAction->SetLocation((long)sAreaCenter.GetX(), (long)sAreaCenter.GetY());
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_SE_OP_CONDITION_TYPE:
        static_cast<EESETSCondition*>(Screen(kSETriggerScreenCondition))->ChangeConditionType();
        break;
    case EE_SCREENS_SE_OP_EFFECT_TYPE:
        static_cast<EESETSEffect*>(Screen(kSETriggerScreenEffect))->ChangeEffectType();
        break;
    case EE_SCREENS_SE_OP_EFFECT_ACTION:
        static_cast<EESETSEffect*>(Screen(kSETriggerScreenEffect))->ChangeEffectAction();
        break;
    case EE_SCREENS_SE_OP_TRIGGER_OPERATOR:
        Trigger()->ChangeOperator(inA);
        break;
    case EE_SCREENS_SE_OP_TRIGGER_STEP:
        switch (inA) {
        case 0: Trigger()->DecrementA4(); break;
        case 1: Trigger()->IncrementA4(); break;
        case 2: Trigger()->DecrementA8(); break;
        default: Trigger()->IncrementA8(); break;
        }
        break;
    case EE_SCREENS_SE_OP_TRIGGER_REMOVE:
        Trigger()->RemoveEntry(inA);
        break;
    case EE_SCREENS_SE_OP_CHECKED:
        return static_cast<UICheckBox*>(theEntity)->GetIsChecked();
    case EE_SCREENS_SE_OP_SHOW:
        if (UIControl* theControl = theEditor->GetControlById(inA))
            theControl->SetDisplayMode(inB);
        break;
    // the editor form's and its tabs' (group 2)
    case EE_SCREENS_SE_OP_UNIT_RESELECT:
        theEditor->GetUnitScreen()->ReselectUnitType();
        break;
    case EE_SCREENS_SE_OP_PLAYER_FILL_TECH:
        theEditor->GetPlayerScreen()->FillTechLists();
        break;
    case EE_SCREENS_SE_OP_HOME_ROTATION: {
        GETransformation theTransformation;
        theTransformation.MakeIdentity();
        gWorld.mCameraControl.SetViewTurn(theTransformation, false);
        theTransformation.SetOrientationYPR(0.7853982f, -0.61086524f, 0.0f);
        gWorld.mCameraControl.SetOffsetTurn(theTransformation, false);
        break;
    }
    case EE_SCREENS_SE_OP_OPEN_CIV_BUILDER:
        return (uint32_t)gUserInterface->OpenCivilizationBuilder(&theEditor->mC1A0, true);
    case EE_SCREENS_SE_OP_LOAD_TRIGGERS: {
        FSFileSpec theFile;
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory);
        theFile.SetFile("ScenarioTriggers.scn");
        if (theFile.Open(kFileAccessRead) == kErrorOK)
            gWorld.mScenarioManager->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
        theFile.Close();
        break;
    }
    case EE_SCREENS_SE_OP_RANGE:
        return (uint32_t)static_cast<UISlider*>(theEditor->GetControlById(inA))->GetCurrentRangeValue();
    case EE_SCREENS_SE_OP_LABEL_NUMBER:
        static_cast<UITextLabel*>(theEditor->GetControlById(inA))->SetText((long)(int32_t)inB);
        break;
    case EE_SCREENS_SE_OP_PLAYER_REBUILD_TECH:
        theEditor->GetPlayerScreen()->RebuildTechLists();
        break;
    case EE_SCREENS_SE_OP_PLACE_FILL: {
        UIComboBox* theComboBox = static_cast<UIComboBox*>(theEditor->GetControlById(0x999));
        FillPlaceComboBox(inA != 0, static_cast<UITextLabel*>(theEditor->GetControlById(0x96d)), theComboBox);
        theComboBox->SetCurrentSelection(0);
        break;
    }
    case EE_SCREENS_SE_OP_IN_EDITOR:
        return gUIManager->GetInScenarioEditor() == true;
    case EE_SCREENS_SE_OP_POST_LOAD_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        theEditor->PostLoadEvent(theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_SE_OP_POST_ACTION:
        if (inA == EE_SCREENS_SE_ACTION_SAVE)
            EESEPostAction(new EESEARust(kSEASave));
        else if (inA == EE_SCREENS_SE_ACTION_TEST)
            EESEPostAction(new EESEARust(kSEATest));
        else
            EESEPostAction(new EESEARust(kSEAExit));
        break;
    case EE_SCREENS_SE_OP_ASK_EXIT: {
        UWideString theText;
        gResource->GetString(IDS_SAVE_SCENARIO_BEFORE_EXITING, theText);
        gUIManager->DisplayYesNoCancel(theText, 1, NULL, EESESaveBeforeExit, EESEExitWithoutSaving);
        break;
    }
    case EE_SCREENS_SE_OP_EXIT_AFTER_SAVE:
        gUIManager->mScenarioEditorForm->mExitAfterSave = true;
        break;
    case EE_SCREENS_SE_OP_EDIT_LINES:
        return static_cast<UIEditBox*>(theEntity)->GetLineCount();
    }
    return 0;
}

// FUNCTION: EXE 0x56fd6a
// Prints the slider value times the scale plus the offset into the event's label
long EESEESliderChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    sprintf(theText, this->mFormat, static_cast<UISlider*>(inEntity)->GetCurrentRangeValue() * this->mScale + this->mOffset);
    this->mLabel->SetText(theText);
    return kErrorOK;
}

// FUNCTION: EXE 0x56fdbf
// Shows the event's controls while the check box is checked, else hides them
long EESEECheckDisplay::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theMode;
    theMode = static_cast<UICheckBox*>(inEntity)->GetIsChecked() ? 1 : 0;
    for (unsigned long i = 0; i < this->mControls.size(); i++)
        this->mControls[i]->SetDisplayMode(theMode);
    return kErrorOK;
}

// FUNCTION: EXE 0x56fe58
// Refills the scenario combo box with area names (area control clicked) or object names and picks the first
long EESEEUseObjectArea::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FillScenarioComboBox(this->mComboBox, this->mLabel, inEntity == this->mAreaControl);
    this->mComboBox->SetCurrentSelection(0);
    return kErrorOK;
}

// FUNCTION: EXE 0x56fe85
// Fills a scenario combo box with the area or object names and titles its label 'Area' or 'Object'
void FillScenarioComboBox(EESETSEScenarioComboBox* inComboBox, UITextLabel* inLabel, bool inAreas)
{
    UWideString theTitle;
    std::vector<UWideString> theNames;
    if (inAreas == true) {
        gWorld.mScenarioManager->GetAreaNames(theNames);
        gResource->GetString(IDS_AREA, theTitle);
        inComboBox->mMode = kSETriggerScreenArea;
    } else {
        gWorld.mScenarioManager->GetObjectNames(theNames);
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theTitle);
        inComboBox->mMode = kSETriggerScreenObject;
    }
    inLabel->SetText(theTitle);
    inComboBox->DeleteAllEntries();
    for (unsigned long i = 0; i < theNames.size(); i++)
        inComboBox->AppendLine(theNames[i]);
}
