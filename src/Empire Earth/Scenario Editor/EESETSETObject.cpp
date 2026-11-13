// C:\EEX\Empire Earth\Scenario Editor\EESETSETObject.cpp (file name inferred: no __FILE__). 0x18849c..0x189904:
// EESETSETObject (ctor, dtor, its six slots), EEScenarioManager::GetScenarioObject's inline COMDAT, the ??_G of the
// two vector-holding object types, EESETSETObjectToArea / EESETSETObjectToObject, and the EESETSETPVisibility slots
// (ICF-shared with the camera fades; by the one-.cpp-per-effect-type rule these may be a unit of their own).
// After EESETSETMSDCameraScrollChange.cpp, before EESETSETPlayer.cpp. Unit flags: the exe module set; include set
// copied from EESETSETAI.cpp (inferred).
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
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSETObject.h"
#include "ee/EESETSETSetObjectStance.h"
#include "ee/EESETSETPlayer.h"
#include "ee/EESMObject.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
// FUNCTION: EXE 0x58849c
// Creates the object effect page with its thirteen object action types (create ... class attribute) and its combo event
EESETSETObject::EESETSETObject()
{
    this->mTypes[kSMObjectActionCreate] = new EESETSETObjectCreate;
    this->mTypes[kSMObjectActionFaceObject] = new EESETSETLogObject;
    this->mTypes[kSMObjectActionFreeze] = new EESETSETFreezeObject;
    this->mTypes[kSMObjectActionUnfreeze] = new EESETSETFreezeObject;
    this->mTypes[kSMObjectActionKill] = new EESETSETKillObject;
    this->mTypes[kSMObjectActionRemove] = new EESETSETRemoveObject;
    this->mTypes[kSMObjectActionTaskObject] = new EESETSETObjectToObject;
    this->mTypes[kSMObjectActionTaskArea] = new EESETSETObjectToArea;
    this->mTypes[kSMObjectActionOwner] = new EESETSETSetObjectOwner;
    this->mTypes[kSMObjectActionUnitAttribute] = new EESETSETSetObjectAttribute;
    this->mTypes[kSMObjectActionStance] = new EESETSETSetObjectStance;
    this->mTypes[kSMObjectActionClassName] = new EESETSETSetObjectName;
    this->mTypes[kSMObjectActionClassAttribute] = new EESETSETSetObjectClassAttribute;
    this->mCurrentType = kSMObjectActionCreate;
    this->mAction = EECreateScreenEvent(EE_SCREENS_SE_CHANGE_EFFECT_ACTION);
}

// SYNTHETIC: EXE 0x588644
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETObject+0x0)

// FUNCTION: EXE 0x588660
// Destroys the object effect page, deleting its thirteen action types and releasing the action-combo event
EESETSETObject::~EESETSETObject()
{
    delete this->mTypes[0];
    delete this->mTypes[11];
    delete this->mTypes[1];
    delete this->mTypes[2];
    delete this->mTypes[3];
    delete this->mTypes[4];
    delete this->mTypes[5];
    delete this->mTypes[6];
    delete this->mTypes[9];
    delete this->mTypes[7];
    delete this->mTypes[10];
    delete this->mTypes[8];
    delete this->mTypes[12];
    this->mAction->Dereference();
}

// FUNCTION: EXE 0x58871b
// Switches the object effect to the action picked in the combo: hides the old action, shows and fills the new, renames
void EESETSETObject::ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    inControls->mComboBoxes[0]->GetCurrentSelection(theSelection);
    this->mCurrentType = theSelection;
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
    this->UpdateName(inEffect, inControls);
}

// FUNCTION: EXE 0x58876a
// Writes the chosen object action, the object and the action's own settings into the effect, then hides the action
void EESETSETObject::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK) {
        inEffect->mObjectAction = theSelection;
        this->mCurrentType = theSelection;
    }
    if (inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mObject = theSelection;
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    this->mTypes[this->mCurrentType]->Store(inEffect, inControls);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
}

// FUNCTION: EXE 0x5887d3
// Hides the object effect's title, action and object labels, the action and object combos and the action's controls
void EESETSETObject::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
}

// FUNCTION: EXE 0x58881d
// Shows an object effect in the editor: selects its action silently, shows the action, selects the object, fills it
void EESETSETObject::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->mCurrentType = inEffect->mObjectAction;
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mComboBoxes[0]->SetCurrentSelection(this->mCurrentType);
    this->mTypes[this->mCurrentType]->Show(inControls);
    inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mObject);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, this->mAction);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
}

// FUNCTION: EXE 0x588890
// Shows the 'Object effect' title, the action combo (object actions) and the object combo filled with scenario objects
void EESETSETObject::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_OBJECT_EFFECT, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 300.0f, theLocation.GetY() + 20.0f));
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
    for (unsigned long theIndex = IDS_TRIGGER_OBJECT_EFFECT_FIRST; theIndex <= IDS_CLASS_ATTRIBUTE; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(220.0f);
    theLocation.SetY(666.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[1]->mMode = kSETriggerScreenObject;
    gWorld.mScenarioManager->GetObjectNames(theNames);
    for (unsigned long i = 0; i < theNames.size(); i++)
        inControls->mScenarioComboBoxes[1]->AppendLine(theNames[i]);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x588aef
// Renames a default-named <...> effect to <action object> plus the stance or attribute picked, and shows it
void EESETSETObject::UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    UWideString theUnused1;
    UWideString theName;
    UWideString theDetail;
    UWideString theUnused2;
    UWideString theObjectName;
    unsigned long theSelection;
    theText = inEffect->mName;
    if (theText.GetLength() > 0 && theText[0] == '<') {
        gResource->GetString(this->mCurrentType + IDS_TRIGGER_OBJECT_EFFECT_FIRST, theText);
        unsigned long thePosition = theText.Find(" ", 0);
        if (thePosition > 0)
            theName.CopyString(theText, thePosition);
        else
            theName = theText;
        theName = "<" + theName;
        inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection);
        EEScenarioManager* theManager = gWorld.mScenarioManager;
        EESMObject* theObject = theManager->GetScenarioObject(theSelection);
        if (theObject)
            theObjectName = theObject->mName;
        else
            gResource->GetString(IDS_NONE, theObjectName);
        switch (this->mCurrentType) {
        case kSMObjectActionCreate:
        case kSMObjectActionFreeze:
        case kSMObjectActionUnfreeze:
        case kSMObjectActionKill:
        case kSMObjectActionRemove:
        case kSMObjectActionTaskObject:
        case kSMObjectActionTaskArea:
        case kSMObjectActionOwner:
        case kSMObjectActionClassName:
        case kSMObjectActionFaceObject:
            theName += " ";
            theName += theObjectName;
            theName += ">";
            break;
        case kSMObjectActionStance:
            inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
            gResource->GetString(theSelection + IDS_TRIGGER_UNIT_BEHAVIOR_FIRST, theDetail);
            theName += " " + theObjectName + " " + theDetail + ">";
            break;
        case kSMObjectActionUnitAttribute:
            inControls->mEffectComboBoxes[0]->GetCurrentSelection(theSelection);
            gResource->GetString(theSelection + IDS_TRIGGER_ATTRIBUTE_FIRST, theDetail);
            theName += " " + theObjectName + " " + theDetail + ">";
            break;
        case kSMObjectActionClassAttribute:
            inControls->mEffectComboBoxes[0]->GetCurrentSelection(theSelection);
            gResource->GetString(theSelection + IDS_TRIGGER_ARMOR_ATTRIBUTE_FIRST, theDetail);
            theName += " " + theObjectName + " " + theDetail + ">";
            break;
        }
        inEffect->mName = theName;
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mNameEditBox->SetText(theName);
    }
}

// FUNCTION: EXE 0x588edd
// Returns the scenario object at an index, or NULL (EEScenarioManager::GetScenarioObject, inline; first emitted here)

// SYNTHETIC: EXE 0x588f0a
// (EESETSETObjectToObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x588f35
// (EESETSETObjectToArea's scalar deleting destructor)

// FUNCTION: EXE 0x588f60
// Stores the task area and the task action (from the action list built by Show) of the object effect
void EESETSETObjectToArea::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mScenarioComboBoxes[3]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mTaskArea = theSelection;
    inControls->mEffectComboBoxes[0]->GetCurrentSelection(theSelection);
    inEffect->mTaskAction = this->mActions[theSelection];
}

// FUNCTION: EXE 0x588f96
// Hides the 'to' label (resetting its alignment), the area and action labels and the area and action combos
void EESETSETObjectToArea::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x588fd9
// Selects the effect's task area and its task action in the action combo (the first action if not listed)
void EESETSETObjectToArea::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    long theValue = 0;
    inControls->mScenarioComboBoxes[3]->SetCurrentSelection(inEffect->mTaskArea);
    theValue = inEffect->mTaskAction;
    if (std::find(this->mActions.begin(), this->mActions.end(), theValue) != this->mActions.end())
        inControls->mEffectComboBoxes[0]->SetCurrentSelection(std::find(this->mActions.begin(), this->mActions.end(), theValue) - this->mActions.begin());
    else if (inControls->mEffectComboBoxes[0]->GetListBox()->GetLineCount() > 0)
        inControls->mEffectComboBoxes[0]->SetCurrentSelection(0);
}

// FUNCTION: EXE 0x5890bd
// Shows 'to', the area combo (scenario areas) and the action combo (normal ... garrison, then every calamity)
void EESETSETObjectToArea::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TO, theText);
    theLocation.SetX(425.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 25.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_AREA, theText);
    theLocation.SetX(inControls->mLabels[5]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 5.0f);
    inControls->mScenarioComboBoxes[3]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[3]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[3]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[3]->mMode = kSETriggerScreenArea;
    gWorld.mScenarioManager->GetAreaNames(theNames);
    for (unsigned long i = 0; i < theNames.size(); i++)
        inControls->mScenarioComboBoxes[3]->AppendLine(theNames[i]);
    inControls->mScenarioComboBoxes[3]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_ACTION, theText);
    theLocation.SetY(inControls->mScenarioComboBoxes[3]->GetRectangle().GetBottom() + 10.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 5.0f);
    inControls->mEffectComboBoxes[0]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[0]->ChangeWidth(200);
    inControls->mEffectComboBoxes[0]->DeleteAllEntries();
    this->mActions.clear();
    inControls->mEffectComboBoxes[0]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_NORMAL; theIndex <= IDS_TRIGGER_GARRISON; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[0]->AppendLine(theText);
        this->mActions.push_back(theIndex - 0x72bf);
    }
    for (unsigned long theClassIndex = 0; theClassIndex <= EECalamityManager::GetInstance()->GetCalamityClassCount();
         theClassIndex++) {
        EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass(theClassIndex);
        if (!theClass)
            continue;
        this->mActions.push_back(theClass->mID + 6);
        gResource->GetString(theClass->mNameID, theText);
        inControls->mEffectComboBoxes[0]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x5893fc
// Stores the task object and the task action (from the action list built by Show) of the object effect
void EESETSETObjectToObject::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mScenarioComboBoxes[2]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mTaskObject = theSelection;
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    inEffect->mTaskAction = this->mActions[theSelection];
}

// FUNCTION: EXE 0x589432
// Hides the 'to' label (resetting its alignment), the object and action labels and the target object and action combos
void EESETSETObjectToObject::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x589475
// Selects the effect's task object and its task action in the action combo (the first action if not listed)
void EESETSETObjectToObject::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    long theValue = inEffect->mTaskAction;
    inControls->mScenarioComboBoxes[2]->SetCurrentSelection(inEffect->mTaskObject);
    if (std::find(this->mActions.begin(), this->mActions.end(), theValue) != this->mActions.end())
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(std::find(this->mActions.begin(), this->mActions.end(), theValue) - this->mActions.begin());
    else if (inControls->mEffectComboBoxes[1]->GetListBox()->GetLineCount() > 0)
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(0);
}

// FUNCTION: EXE 0x589552
// Shows 'to', the target object combo (scenario objects) and the action combo (normal ... garrison, every calamity)
void EESETSETObjectToObject::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TO, theText);
    theLocation.SetX(425.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 25.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
    theLocation.SetX(inControls->mLabels[5]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 5.0f);
    inControls->mScenarioComboBoxes[2]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[2]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[2]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[2]->mMode = kSETriggerScreenObject;
    gWorld.mScenarioManager->GetObjectNames(theNames);
    for (unsigned long i = 0; i < theNames.size(); i++)
        inControls->mScenarioComboBoxes[2]->AppendLine(theNames[i]);
    inControls->mScenarioComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_ACTION, theText);
    theLocation.SetY(inControls->mScenarioComboBoxes[2]->GetRectangle().GetBottom() + 10.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 5.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(200);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    this->mActions.clear();
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_NORMAL; theIndex <= IDS_TRIGGER_GARRISON; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
        this->mActions.push_back(theIndex - 0x72bf);
    }
    for (unsigned long theClassIndex = 0; theClassIndex <= EECalamityManager::GetInstance()->GetCalamityClassCount();
         theClassIndex++) {
        EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass(theClassIndex);
        if (!theClass)
            continue;
        this->mActions.push_back(theClass->mID + 6);
        gResource->GetString(theClass->mNameID, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x589891
// Selects the effect's player without firing the picker event (shared with the camera fade actions)
void EESETSETPVisibility::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mPlayers[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mPlayer);
    if (theAction) {
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x5898eb
// Sets the player picker's flags for the visibility, camera fade and victory actions (shared body)
void EESETSETPVisibility::Show(EESETSEffectControls* inControls)
{
    inControls->mPlayers[0]->SetFlags(0xde);
}
