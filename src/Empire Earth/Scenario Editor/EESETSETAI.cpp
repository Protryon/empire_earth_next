// C:\EEX\Empire Earth\Scenario Editor\EESETSETAI.cpp (file name inferred: no __FILE__). EESETSETAI and its seven AI
// effect types: 0x182dda..0x1843c7 (EESETSETAI ctor..Virtual18, the types' implicit ctor/dtor COMDATs, EESETSETBuildObject,
// EESETSETCAMGModifyObject::ProcessEvent, EESETSETCreateAIMilitaryGroup incl. Virtual14 0x183d81, and the ??_G COMDATs
// of EESETSETCAMGModifyObject and EESEEUseObjectArea). After EESETSEffect.cpp, before EESETSETGame.cpp. Unit flags: the
// exe module set. Merged with the former EESETSETCAMGModifyObject.cpp
// (0x183d81..0x1843c7): one original .obj, since ??_GEESEEUseObjectArea 0x58439c is emitted only by this file's
// `new EESEEUseObjectArea` (exact in the merged unit, error in the split one).
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
#include "ee/EESETSETAI.h"
#include "ee/EESETSETCAMGModifyObject.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
// Original16fe85; same actual declaration used by the camera-scroll controls.
void FillScenarioComboBox(EESETSEScenarioComboBox* inComboBox, UITextLabel* inLabel, bool inAreas);

// FUNCTION: EXE 0x582dda
// Creates the AI effect page with its seven AI action types and the action combo's change event
EESETSETAI::EESETSETAI()
{
    this->mTypes[kSMAIActionModifyAIMilitaryTarget] = new EESETSETModifyAIMilitaryTarget;
    this->mTypes[kSMAIActionCreateAIMilitaryGroup] = new EESETSETCreateAIMilitaryGroup;
    this->mTypes[kSMAIActionBuildObject] = new EESETSETBuildObject;
    this->mTypes[kSMAIActionMaintainObject] = new EESETSETMaintainObject;
    this->mTypes[kSMAIActionSetAIToggle] = new EESETSETToggleAI;
    this->mTypes[kSMAIActionSetAIVariable] = new EESETSETSetAIAttribute;
    this->mTypes[kSMAIActionSetFavoriteUnits] = new EESETSETSetFavoriteUnit;
    this->mCurrentType = 0;
    this->mAction = new EESEEChangeEffectAction;
}

// SYNTHETIC: EXE 0x582edb
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETAI+0x0)

// FUNCTION: EXE 0x582ef7
// Destroys the AI effect page, deleting its seven action types and releasing the action-combo event
EESETSETAI::~EESETSETAI()
{
    delete this->mTypes[0];
    delete this->mTypes[1];
    delete this->mTypes[2];
    delete this->mTypes[3];
    delete this->mTypes[4];
    delete this->mTypes[5];
    delete this->mTypes[6];
    this->mAction->Dereference();
}

// FUNCTION: EXE 0x582f6a
// Switches the AI effect to the action picked in the combo: hides the old type's controls, shows and fills the new one
void EESETSETAI::ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    inControls->mComboBoxes[0]->GetCurrentSelection(theSelection);
    this->mCurrentType = theSelection;
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
    this->UpdateName(inEffect, inControls);
}

// FUNCTION: EXE 0x582fb9
// Writes the chosen AI action, player and the action's own settings into the effect, then hides the action's controls
void EESETSETAI::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK) {
        inEffect->mAIAction = theSelection;
        this->mCurrentType = theSelection;
    }
    inEffect->mPlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    this->mTypes[this->mCurrentType]->Store(inEffect, inControls);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
}

// FUNCTION: EXE 0x58301c
// Hides the AI effect's title, action label, action combo and player combo plus the current action's controls
// (hides mPlayers[2] instead of the 'Player' label mLabels[2] that Show displays: an original bug)
void EESETSETAI::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[2]->SetDisplayMode(kDisplayModeHidden);
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
}

// FUNCTION: EXE 0x583066
// Shows an AI effect in the editor: selects its action and player without firing their events, then shows the action
void EESETSETAI::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->mCurrentType = inEffect->mAIAction;
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mComboBoxes[0]->SetCurrentSelection(this->mCurrentType);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, this->mAction);
    UIEvent* theEvent = inControls->mPlayers[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent)
        theEvent->Reference();
    inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mPlayer);
    if (theEvent) {
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
}

// FUNCTION: EXE 0x583114
// Lays out and shows the 'AI effect' title, the action label and combo (filled with the AI actions) and player picker
void EESETSETAI::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_AI_EFFECT, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 206.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_ACTION, theText);
    theLocation.SetX(10.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[1]->SetRectangle(theRect);
    inControls->mLabels[1]->SetText(theText);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(10.0f);
    theLocation.SetY(666.0f);
    inControls->mComboBoxes[0]->SetLocation(theLocation);
    inControls->mComboBoxes[0]->ChangeWidth(200);
    inControls->mComboBoxes[0]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_AI_EFFECT_FIRST; theIndex <= IDS_SET_FAVORITE_UNITS; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(220.0f);
    theLocation.SetY(666.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0x94);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58330a
// Renames an effect that still has a default <...> name to <first word of its AI action> and shows it in the name box
void EESETSETAI::UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    UWideString theName;
    theText = inEffect->mName;
    if (theText.GetLength() > 0 && theText[0] == '<') {
        gResource->GetString(this->mCurrentType + IDS_TRIGGER_AI_EFFECT_FIRST, theText);
        unsigned long thePosition = theText.Find(" ", 0);
        if (thePosition > 0)
            theName.CopyString(theText, thePosition);
        else
            theName = theText;
        theText = "<" + theName + ">";
        inEffect->mName = theText;
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mNameEditBox->SetText(theText);
    }
}

// SYNTHETIC: EXE 0x583429
// (EESEEChangeEffectAction's scalar deleting destructor)

// SYNTHETIC: EXE 0x583454
// (EESETSETSetAIAttribute's scalar deleting destructor, its implicit dtor inline)

// FUNCTION: EXE 0x58347f
// Creates the create-AI-military-group action type with empty object lists (implicit ctor, out of line: refused inline)

// SYNTHETIC: EXE 0x5834b5
// (EESETSETCreateAIMilitaryGroup's scalar deleting destructor)

// FUNCTION: EXE 0x5834d1
// Destroys the create-AI-military-group action type, freeing its two object lists (implicit dtor)

// FUNCTION: EXE 0x5834f2
// Creates the set-favorite-units action type with empty air, land and sea unit lists (implicit ctor, refused inline)

// SYNTHETIC: EXE 0x583537
// (EESETSETSetFavoriteUnit's scalar deleting destructor)

// FUNCTION: EXE 0x583553
// Destroys the set-favorite-units action type, freeing its air, land and sea unit lists (implicit dtor)

// FUNCTION: EXE 0x58357c
// Stores the object chosen in the object combo as the effect's target (shared by the build and maintain object actions)
void EESETSETBuildObject::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mScenarioComboBoxes[1]->GetCurrentSelection(inEffect->mTargetObject);
}

// FUNCTION: EXE 0x583596
// Selects the effect's target object in the object combo without firing its selection-changed event
void EESETSETBuildObject::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theEvent = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theEvent)
        theEvent->Reference();
    inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mTargetObject);
    if (theEvent) {
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
}

// FUNCTION: EXE 0x5835f1
// Shows the 'Object' label and the object combo filled with the scenario's object names (build and maintain object)
void EESETSETBuildObject::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    std::vector<UWideString> theNames;
    theLocation.SetX(405.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[1]->mMode = kSETriggerScreenObject;
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    gWorld.mScenarioManager->GetObjectNames(theNames);
    inControls->mScenarioComboBoxes[1]->AppendStringArray(theNames);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58370d
// Moves the clicked object between the 'to add' list (id 0x9bf) and the 'added' list of the military group action
long EESETSETCAMGModifyObject::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UILBString* theList = static_cast<UILBString*>(inEntity);
    unsigned long theSelection;
    if (theList->GetCurrentSelection(theSelection) != kErrorOK)
        return kErrorOK;
    if (theList->GetId() == 0x9bf)
        this->mType->AddObject(theSelection, this->mFirst, this->mSecond);
    else
        this->mType->RemoveObject(theSelection, this->mFirst, this->mSecond);
    return kErrorOK;
}

// FUNCTION: EXE 0x583753
// Moves an object from the 'to add' list into the 'added' list, keeping the added list sorted by object index
void EESETSETCreateAIMilitaryGroup::AddObject(unsigned long inSelection, UILBString* inFirst, UILBString* inSecond)
{
    UWideString theText(inFirst->GetData(inSelection));
    unsigned long theObject = this->mObjectsToAdd[inSelection];
    unsigned long theIndex = 0;
    while (theIndex < this->mObjectsAdded.size()) {
        if (theObject < this->mObjectsAdded[theIndex])
            break;
        theIndex++;
    }
    inSecond->InsertLine(theText, theIndex);

    this->mObjectsAdded.insert(this->mObjectsAdded.begin() + theIndex, theObject);
    inFirst->DeleteLine(inSelection);
    this->mObjectsToAdd.erase(this->mObjectsToAdd.begin() + inSelection);
}

// FUNCTION: EXE 0x58386e
// Stores the group's target area or object, attack/favorites flags and added objects, then detaches the radio events
void EESETSETCreateAIMilitaryGroup::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection);
    if (inControls->mRadioButtons[0]->GetIsChecked() == true) {
        inEffect->mUsesTargetArea = true;
        inEffect->mTargetArea = theSelection;
    } else {
        inEffect->mUsesTargetArea = false;
        inEffect->mTargetObject = theSelection;
    }
    inEffect->mIsAttack = inControls->mCheckBoxes[0]->GetIsChecked();
    inEffect->mUsesFavorites = inControls->mCheckBoxes[1]->GetIsChecked();
    inEffect->mObjects = this->mObjectsAdded;
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x583904
// Hides the military group controls and detaches their events
void EESETSETCreateAIMilitaryGroup::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetAlignment(kTextAlignLeft);
    inControls->mListBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mListBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    // Show attaches the add/remove handler to button-up: clear that one (button-down was cleared).
    inControls->mListBoxes[0]->SetTriggerEvent(kControlTriggerButtonUp, NULL);
    inControls->mListBoxes[1]->SetTriggerEvent(kControlTriggerButtonUp, NULL);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mListBoxes[1]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x5839da
// Fills the military group controls from the effect: target area or object, flags, 'to add' and 'added' object lists
void EESETSETCreateAIMilitaryGroup::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    std::vector<UWideString> theNames;
    std::vector<long> theUnused;
    UIEvent* theEvent;
    if (inEffect->mUsesTargetArea) {
        inControls->mRadioButtons[0]->Check();
        FillScenarioComboBox(inControls->mScenarioComboBoxes[1], inControls->mLabels[3], true);
        theEvent = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theEvent) theEvent->Reference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mTargetArea);
    } else {
        inControls->mRadioButtons[1]->Check();
        FillScenarioComboBox(inControls->mScenarioComboBoxes[1], inControls->mLabels[3], false);
        theEvent = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theEvent) theEvent->Reference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mTargetObject);
    }
    if (theEvent) {
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
    }
    if (inControls->mCheckBoxes[0]->GetIsChecked() != inEffect->mIsAttack)
        inControls->mCheckBoxes[0]->SetChecked(inEffect->mIsAttack);
    if (inControls->mCheckBoxes[1]->GetIsChecked() != inEffect->mUsesFavorites)
        inControls->mCheckBoxes[1]->SetChecked(inEffect->mUsesFavorites);
    this->mObjectsToAdd.clear();
    this->mObjectsAdded.clear();
    inControls->mListBoxes[1]->DeleteAllEntries();
    inControls->mListBoxes[0]->DeleteAllEntries();
    this->mObjectsAdded = inEffect->mObjects;
    gWorld.mScenarioManager->GetObjectNames(theNames);
    for (unsigned long i = 1; i < theNames.size(); i++) {
        UILBString* theList;
        if (std::find(this->mObjectsAdded.begin(), this->mObjectsAdded.end(), i) == this->mObjectsAdded.end()) {
            this->mObjectsToAdd.push_back(i);
            theList = inControls->mListBoxes[0];
        } else
            theList = inControls->mListBoxes[1];
        theList->AppendLine(theNames[i]);
    }
    theEvent = new EESEEUseObjectArea(inControls->mLabels[3], inControls->mScenarioComboBoxes[1],
                                     inControls->mRadioButtons[0]);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
}

// FUNCTION: EXE 0x583c66
// Moves an object from the 'added' list back into the 'to add' list, keeping that list sorted by object index
void EESETSETCreateAIMilitaryGroup::RemoveObject(unsigned long inSelection, UILBString* inFirst, UILBString* inSecond)
{
    UWideString theText(inSecond->GetData(inSelection));
    unsigned long theObject = this->mObjectsAdded[inSelection];
    unsigned long theIndex = 0;
    while (theIndex < this->mObjectsToAdd.size()) {
        if (theObject < this->mObjectsToAdd[theIndex])
            break;
        theIndex++;
    }
    inFirst->InsertLine(theText, theIndex);

    this->mObjectsToAdd.insert(this->mObjectsToAdd.begin() + theIndex, theObject);
    inSecond->DeleteLine(inSelection);
    this->mObjectsAdded.erase(this->mObjectsAdded.begin() + inSelection);
}

// NEAR: the original hoists SetTriggerEvent into edi, ours SetScrollBarBgOpacity (register choice)
// FUNCTION: EXE 0x583d81
// Shows the attack/favorites boxes, area/object radios, target combo and the two object lists with their click event
void EESETSETCreateAIMilitaryGroup::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    GEText* theGEText;
    EESETSETCAMGModifyObject* theEvent;
    theLocation.SetX(4.0f);
    theLocation.SetY(692.0f);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    gResource->GetString(IDS_SE_ATTACK, theText);
    inControls->mCheckBoxes[0]->SetText(theText, gU531934);
    inControls->mCheckBoxes[0]->ChangeWidth(0x55);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    theLocation.SetY(inControls->mCheckBoxes[0]->GetRectangle().GetBottom() + 1.0f);
    inControls->mCheckBoxes[1]->SetLocation(theLocation);
    gResource->GetString(IDS_FAVORITES, theText);
    inControls->mCheckBoxes[1]->SetText(theText, gU531934);
    inControls->mCheckBoxes[1]->ChangeWidth(0x55);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theGEText = inControls->mCheckBoxes[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    theLocation.SetX(inControls->mCheckBoxes[0]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(692.0f);
    gResource->GetString(IDS_USE_AREA, theText);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[0]->SetLocation(theLocation);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mRadioButtons[0]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, theText);
    inControls->mRadioButtons[1]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[1]->SetLocation(theLocation);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mRadioButtons[0]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(692.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(UWideString("----"));
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(175);
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_OBJECTS_TO_ADD, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 195.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_OBJECTS_ADDED, theText);
    theLocation.SetX(inControls->mLabels[4]->GetRectangle().GetRight() + 1.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 195.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetAlignment(kTextAlignRight);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[4]->GetRectangle().GetLeft());
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 195.0f, theLocation.GetY() + 90.0f));
    inControls->mListBoxes[0]->SetRectangle(theRect);
    inControls->mListBoxes[0]->SetBgOpacity(0.4f);
    inControls->mListBoxes[0]->SetScrollBarBgOpacity(0.4f);
    theEvent = new EESETSETCAMGModifyObject(this, inControls->mListBoxes[0], inControls->mListBoxes[1]);
    inControls->mListBoxes[0]->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theGEText = inControls->mListBoxes[0]->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mListBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mListBoxes[0]->GetRectangle().GetRight() + 1.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 195.0f, theLocation.GetY() + 90.0f));
    inControls->mListBoxes[1]->SetRectangle(theRect);
    inControls->mListBoxes[1]->SetBgOpacity(0.4f);
    inControls->mListBoxes[1]->SetScrollBarBgOpacity(0.4f);
    inControls->mListBoxes[1]->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theGEText = inControls->mListBoxes[1]->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mListBoxes[1]->SetDisplayMode(kDisplayModeShown);
    inControls->mListBoxes[1]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeHidden);
}

// SYNTHETIC: EXE 0x584371
// (EESETSETCAMGModifyObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x58439c
// (EESEEUseObjectArea's scalar deleting destructor; ICF-kept copy, no user of it in this unit is known)
