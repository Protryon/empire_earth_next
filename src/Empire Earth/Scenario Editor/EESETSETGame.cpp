// C:\EEX\Empire Earth\Scenario Editor\EESETSETGame.cpp (file name inferred: no __FILE__). 0x1843c7..0x185d31:
// EESETSETGame and its game effect types. Unit flags: the exe module set.
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
#include "ee/EESETSETGame.h"
#include "ee/EESETSETObject.h"
#include "ee/EESETSETAI.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
// FUNCTION: EXE 0x5843c7
// Creates the game effect page with its seven game action types (daylight hour ... ambient weather) and its combo event
EESETSETGame::EESETSETGame()
{
    this->mTypes[kSMGameActionDaylightHour] = new EESETSETGameDayNight;
    this->mTypes[kSMGameActionDayPerNightCycleTime] = new EESETSETGameCycleTime;
    this->mTypes[kSMGameActionCalamity] = new EESETSETGameCalamity;
    this->mTypes[kSMGameActionGraphicEffect] = new EESETSETGameGraphicEffect;
    this->mTypes[kSMGameActionStartCinematicMode] = new EESETSETGameStartCinematic;
    this->mTypes[kSMGameActionEndCinematicMode] = new EESETSETGameEndCinematic;
    this->mTypes[kSMGameActionAmbientWeather] = new EESETSETGameAmbientWeather;
    this->mCurrentType = 0;
    this->mAction = new EESEEChangeEffectAction;
}

// SYNTHETIC: EXE 0x5844df
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETGame+0x0)

// FUNCTION: EXE 0x5844fb
// Destroys the game effect page, deleting its seven action types and releasing the action-combo event
EESETSETGame::~EESETSETGame()
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

// FUNCTION: EXE 0x58456e
// Writes the chosen game action and the action's own settings into the effect, then hides the action's controls
void EESETSETGame::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK) {
        inEffect->mGameAction = theSelection;
        this->mCurrentType = theSelection;
    }
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    this->mTypes[this->mCurrentType]->Store(inEffect, inControls);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
}

// FUNCTION: EXE 0x5845c3
// Hides the game effect's title, action label and action combo plus the current action's controls
void EESETSETGame::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
}

// FUNCTION: EXE 0x5845fa
// Shows a game effect in the editor: selects its action without firing the combo event, then shows and fills the action
void EESETSETGame::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->mCurrentType = inEffect->mGameAction;
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mComboBoxes[0]->SetCurrentSelection(this->mCurrentType);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, this->mAction);
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
}

// FUNCTION: EXE 0x58465e
// Lays out and shows the 'Game effect' title, the action label and the action combo filled with the game actions
void EESETSETGame::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_GAME_EFFECT, theText);
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
    for (unsigned long theIndex = IDS_TRIGGER_GAME_EFFECT_FIRST; theIndex <= IDS_AMBIENT_WEATHER; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x5847bf
// Renames an effect that still has a default <...> name to <first word of its game action> and shows it in the name box
void EESETSETGame::UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    UWideString theName;
    theText = inEffect->mName;
    if (theText.GetLength() > 0 && theText[0] == '<') {
        gResource->GetString(this->mCurrentType + IDS_TRIGGER_GAME_EFFECT_FIRST, theText);
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

// SYNTHETIC: EXE 0x5848de
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETGameCalamity+0x0, stores EESETSETGameCalamity vtable, stores EESETSEffectType vtable)

// SYNTHETIC: EXE 0x584909
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETGameGraphicEffect+0x0, stores EESETSETGameGraphicEffect vtable, stores EESETSEffectType vtable)

// FUNCTION: EXE 0x584934
// Stores whether ambient weather is switched on, from the check box
void EESETSETGameAmbientWeather::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mIsOn = inControls->mCheckBoxes[0]->GetIsChecked();
}

// FUNCTION: EXE 0x584951
// Hides the ambient weather check box
void EESETSETGameAmbientWeather::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x584965
// Ticks the ambient weather check box from the effect
void EESETSETGameAmbientWeather::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mCheckBoxes[0]->SetChecked(inEffect->mIsOn);
}

// FUNCTION: EXE 0x584985
// Lays out and shows the 'active ambient weather' check box with yellow transparent text
void EESETSETGameAmbientWeather::Show(EESETSEffectControls* inControls)
{
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_ACTIVE_AMBIENT_WEATHER, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(666.0f);
    inControls->mCheckBoxes[0]->SetText(theText, gU531938);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->ChangeWidth(200);
    GEText* theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x584a41
// Refills the calamity's place combo with area names (area radio, id 0x988) or object names and selects the first
// EESEEGCPlace::ProcessEvent below; FillPlaceComboBox (0x184a90) fills the place label and combo box of the
// calamity type: area names (inAreas) or object names
static void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox);

long EESEEGCPlace::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControlById(0x96d));
    UIComboBox* theComboBox = static_cast<UIComboBox*>(inForm->GetControlById(0x999));
    FillPlaceComboBox(static_cast<UIControl*>(inEntity)->GetId() == 0x988, theLabel, theComboBox);
    theComboBox->SetCurrentSelection(0);
    return kErrorOK;
}

// FUNCTION: EXE 0x584a90
// Sets the place label to 'Area' or 'Object' and fills the place combo with the scenario's area or object names
static void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox)
{
    std::vector<UWideString> theNames;
    UWideString theTitle;
    if (inAreas) {
        gResource->GetString(IDS_AREA, theTitle);
        gWorld.mScenarioManager->GetAreaNames(theNames);
    } else {
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theTitle);
        gWorld.mScenarioManager->GetObjectNames(theNames);
    }
    inLabel->SetText(theTitle);
    inComboBox->DeleteAllEntries();
    for (unsigned long i = 0; i < theNames.size(); i++)
        inComboBox->AppendLine(theNames[i]);
}

// FUNCTION: EXE 0x584b58
// Stores the chosen calamity and its place area or object, then detaches the area/object radio events
void EESETSETGameCalamity::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mPlacedEffect = this->mCalamities[theSelection];
    inEffect->mUsesPlaceArea = inControls->mRadioButtons[0]->GetIsChecked();
    if (inControls->mComboBoxes[2]->GetCurrentSelection(theSelection) == kErrorOK) {
        if (inEffect->mUsesPlaceArea)
            inEffect->mPlaceArea = theSelection;
        else
            inEffect->mPlaceObject = theSelection;
    }
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x584bd6
// Selects the effect's calamity (else the first), checks area or object and selects its place; sets the radio event
void EESETSETGameCalamity::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    if (std::find(this->mCalamities.begin(), this->mCalamities.end(), inEffect->GetPlacedEffect()) != this->mCalamities.end())
        inControls->mComboBoxes[1]->SetCurrentSelection(std::find(this->mCalamities.begin(), this->mCalamities.end(), inEffect->GetPlacedEffect()) - this->mCalamities.begin());
    else if (inControls->mComboBoxes[1]->GetListBox()->GetLineCount() > 0)
        inControls->mComboBoxes[1]->SetCurrentSelection(0);
    if (inEffect->mUsesPlaceArea) {
        inControls->mRadioButtons[0]->Check();
        FillPlaceComboBox(inEffect->mUsesPlaceArea, inControls->mLabels[3], inControls->mComboBoxes[2]);
        inControls->mComboBoxes[2]->SetCurrentSelection(inEffect->mPlaceArea);
    } else {
        FillPlaceComboBox(inEffect->mUsesPlaceArea, inControls->mLabels[3], inControls->mComboBoxes[2]);
        inControls->mRadioButtons[1]->Check();
        inControls->mComboBoxes[2]->SetCurrentSelection(inEffect->mPlaceObject);
    }
    UIEvent* theEvent = new EESEEGCPlace;
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
}

// FUNCTION: EXE 0x584d8a
// Deletes an area/object radio event of the calamity place (EESEEGCPlace scalar deleting destructor)
// (the inline empty dtor; ee/EESEEGCPlace.h; emitted with the vtable by EESETSETGameCalamity::Load's `new`)

// FUNCTION: EXE 0x584db5
// Shows the calamity combo, area/object radios and place combo; rebuilds mCalamities with the combo
void EESETSETGameCalamity::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TRIGGER_EDITOR_CALAMITY, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mComboBoxes[1]->DeleteAllEntries();
    this->mCalamities.clear();      // rebuilt with the combo (it grew by the list length on every Show)
    for (unsigned long theIndex = 0; theIndex <= EECalamityManager::GetInstance()->GetCalamityClassCount(); theIndex++) {
        EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass(theIndex);
        if (!theClass)
            continue;
        this->mCalamities.push_back(theClass->mID);
        gResource->GetString(theClass->mNameID, theText);
        inControls->mComboBoxes[1]->AppendLine(theText);
    }
    inControls->mComboBoxes[1]->ChangeWidth(200);
    inControls->mComboBoxes[1]->SetLocation(theLocation);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[2]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(616.0f);
    gResource->GetString(IDS_USE_AREA, theText);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    GEText* theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
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
    theLocation.SetY(666.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(UWideString(""));
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[2]->DeleteAllEntries();
    inControls->mComboBoxes[2]->ChangeWidth(200);
    inControls->mComboBoxes[2]->SetLocation(theLocation);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58509d
// Stores the day/night cycle time typed in the edit box
void EESETSETGameCycleTime::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mCycleTime = inControls->mEditBoxes[0]->GetTextAsInt(0);
}

// FUNCTION: EXE 0x5850c1
// Hides the cycle time label, edit box and 'seconds' label (realigning the first label left)
void EESETSETGameCycleTime::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x5850f2
// Fills the cycle time edit box from the effect
void EESETSETGameCycleTime::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inControls->mEditBoxes[0]->SetText(inEffect->mCycleTime);
}

// FUNCTION: EXE 0x58510c
// Lays out and shows the 'day/night cycle time' label, the edit box (reset to 0) and the 'seconds' label
void EESETSETGameCycleTime::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TRIGGER_EDITOR_DAY_NIGHT_CYCLE_TIME, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[2]->GetRectangle().GetLeft() + 50.0f);
    theLocation.SetY(inControls->mLabels[2]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SECONDS, theText);
    theLocation.SetX(inControls->mEditBoxes[0]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 90.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58526f
// Stores the daylight hour slider position in the effect
void EESETSETGameDayNight::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mDaylightHour = inControls->mSliders[0]->GetCurrentRangeValue();
}

// FUNCTION: EXE 0x58528c
// Moves the daylight hour slider to the effect's hour if it differs
void EESETSETGameDayNight::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    if (inControls->mSliders[0]->GetCurrentRangeValue() != inEffect->mDaylightHour)
        inControls->mSliders[0]->SetCurrentRangeValue(inEffect->mDaylightHour);
}

// FUNCTION: EXE 0x5852b2
// Shows the '24 hour time' label, an hour slider (0..23) and a value label the slider event keeps up to date
void EESETSETGameDayNight::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_24_HOUR_TIME, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 100.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[2]->GetRectangle().GetBottom() + 5.0f);
    inControls->mSliders[0]->SetRange(0, 0x17);
    inControls->mSliders[0]->SetLocation(theLocation);
    inControls->mSliders[0]->SetPageSize(1);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeShown);
    UIEvent* theEvent = new EESEESliderChange(inControls->mLabels[3], UWideString("%2.0f"), 1.0f, 0.0f);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetX(inControls->mSliders[0]->GetRectangle().GetLeft() + 40.0f);
    theLocation.SetY(inControls->mSliders[0]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 20.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(inControls->mSliders[0]->GetCurrentRangeValue());
    inControls->mLabels[3]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58547c
// Refills the graphic effect's place combo with area names (area radio, id 0x988) or object names and selects the first
long EESEEPlace::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControlById(0x96d));
    UIComboBox* theComboBox = static_cast<UIComboBox*>(inForm->GetControlById(0x999));
    FillPlaceComboBox(static_cast<UIControl*>(inEntity)->GetId() == 0x988, theLabel, theComboBox);
    theComboBox->SetCurrentSelection(0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5854cb
// Hides the calamity label and combo, the area/object radios and the place label and combo; detaches the radio events
void EESETSETGameCalamity::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x585537
// Selects the effect's graphic effect (else the first), sets the radio event, checks area or object, selects its place
void EESETSETGameGraphicEffect::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    if (std::find(this->mGraphicEffects.begin(), this->mGraphicEffects.end(), inEffect->GetPlacedEffect()) != this->mGraphicEffects.end())
        inControls->mComboBoxes[1]->SetCurrentSelection(std::find(this->mGraphicEffects.begin(), this->mGraphicEffects.end(), inEffect->GetPlacedEffect()) - this->mGraphicEffects.begin());
    else if (inControls->mComboBoxes[1]->GetListBox()->GetLineCount() > 0)
        inControls->mComboBoxes[1]->SetCurrentSelection(0);
    if (theAction) {
        inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    UIEvent* theEvent = new EESEEPlace;
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
    if (inEffect->mUsesPlaceArea) {
        FillPlaceComboBox(inEffect->mUsesPlaceArea, inControls->mLabels[3], inControls->mComboBoxes[2]);
        UIEvent* thePlace = inControls->mRadioButtons[0]->GetTriggerEvent(kControlTriggerChecked);
        thePlace->Reference();
        inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
        inControls->mRadioButtons[0]->Check();
        inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, thePlace);
        thePlace->Dereference();
        inControls->mComboBoxes[2]->SetCurrentSelection(inEffect->mPlaceArea);
    } else {
        FillPlaceComboBox(inEffect->mUsesPlaceArea, inControls->mLabels[3], inControls->mComboBoxes[2]);
        UIEvent* thePlace = inControls->mRadioButtons[1]->GetTriggerEvent(kControlTriggerChecked);
        thePlace->Reference();
        inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
        inControls->mRadioButtons[1]->Check();
        inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, thePlace);
        thePlace->Dereference();
        inControls->mComboBoxes[2]->SetCurrentSelection(inEffect->mPlaceObject);
    }
}

// FUNCTION: EXE 0x585765
// Deletes an area/object radio event of the graphic effect place (EESEEPlace scalar deleting destructor)
// (the inline empty dtor; ee/EESEEPlace.h; emitted with the vtable by EESETSETGameGraphicEffect::Load's `new`)

// FUNCTION: EXE 0x585790
// Shows the listed graphic effects, radios and place combo; rebuilds mGraphicEffects with the combo
void EESETSETGameGraphicEffect::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_GAME_GRAPHIC_EFFECT, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mComboBoxes[1]->DeleteAllEntries();
    this->mGraphicEffects.clear();  // rebuilt with the combo (it grew by the list length on every Show)
    for (unsigned long theIndex = 0; theIndex < EEGraphicEffect::GetCount(); theIndex++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::Get(theIndex);
        if (!theEffect || theEffect->mIsScenarioListed != true)
            continue;
        this->mGraphicEffects.push_back(theEffect->mID);
        gResource->GetString(theEffect->mNameStringId, theText);
        inControls->mComboBoxes[1]->AppendLine(theText);
    }
    inControls->mComboBoxes[1]->ChangeWidth(200);
    inControls->mComboBoxes[1]->SetLocation(theLocation);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[2]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(616.0f);
    gResource->GetString(IDS_USE_AREA, theText);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    GEText* theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
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
    theLocation.SetY(666.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(UWideString(""));
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[2]->DeleteAllEntries();
    inControls->mComboBoxes[2]->ChangeWidth(200);
    inControls->mComboBoxes[2]->SetLocation(theLocation);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x585a71
// Returns the number of graphic effect types (EEGraphicEffect::GetCount)
// (inline, ee/EEGraphicEffect.h; out of line here, its first user)

// FUNCTION: EXE 0x585a89
// Hides the daylight hour label, slider and value label and detaches the slider event
void EESETSETGameDayNight::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mLabels[3]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x585adc
// Shows the effect's game speed as (slider + 1) * 0.1 and moves the speed slider to it
void EESETSETGameStartCinematic::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    sprintf(theText, "%#2.1f", ((float)inEffect->mDaylightHour + 1.0f) * 0.1f);
    inControls->mLabels[3]->SetText(theText);
    if (inControls->mSliders[0]->GetCurrentRangeValue() != inEffect->mDaylightHour)
        inControls->mSliders[0]->SetCurrentRangeValue(inEffect->mDaylightHour);
}

// FUNCTION: EXE 0x585b5f
// Shows the 'game speed' label, a speed slider (0..29, shown as 0.1..3.0) and its value label
void EESETSETGameStartCinematic::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_MSG_GAME_SPEED, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 100.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[2]->GetRectangle().GetBottom() + 5.0f);
    inControls->mSliders[0]->SetRange(0, 0x1d);
    inControls->mSliders[0]->SetLocation(theLocation);
    inControls->mSliders[0]->SetPageSize(1);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeShown);
    UIEvent* theEvent = new EESEESliderChange(inControls->mLabels[3], UWideString("%#2.1f"), 0.1f, 0.1f);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetX(inControls->mSliders[0]->GetRectangle().GetLeft() + 40.0f);
    theLocation.SetY(inControls->mSliders[0]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 40.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(inControls->mSliders[0]->GetCurrentRangeValue());
    inControls->mLabels[3]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
}

// ICF-FOLDED: EXE 0x584b58
// Stores the chosen graphic effect and its place area or object, then detaches the radio events
// (identical to EESETSETGameCalamity::Store)
void EESETSETGameGraphicEffect::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mPlacedEffect = this->mGraphicEffects[theSelection];
    inEffect->mUsesPlaceArea = inControls->mRadioButtons[0]->GetIsChecked();
    if (inControls->mComboBoxes[2]->GetCurrentSelection(theSelection) == kErrorOK) {
        if (inEffect->mUsesPlaceArea)
            inEffect->mPlaceArea = theSelection;
        else
            inEffect->mPlaceObject = theSelection;
    }
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// ICF-FOLDED: EXE 0x58526f
// Stores the game speed slider position in the effect (identical to EESETSETGameDayNight::Store)
void EESETSETGameStartCinematic::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mDaylightHour = inControls->mSliders[0]->GetCurrentRangeValue();
}

// ICF-FOLDED: EXE 0x5854cb
// Hides the graphic effect controls and detaches the radio events (identical to EESETSETGameCalamity::VirtualC)
void EESETSETGameGraphicEffect::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// ICF-FOLDED: EXE 0x585a89
// Hides the game speed label, slider and value label (identical to EESETSETGameDayNight::VirtualC)
void EESETSETGameStartCinematic::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mLabels[3]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
}

// ICF-FOLDED: EXE 0x585f75
// Hides the object label and combo box (identical to EESETSETBuildObject::VirtualC)
void EESETSETMaintainObject::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}
