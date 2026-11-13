// C:\EEX\Empire Earth\Scenario Editor\EESETSETMSDCameraScrollChange.cpp (file name inferred: no __FILE__; by the
// one-.cpp-per-effect-type rule this range may really be several units: MediaSendDialog's Virtual14 + ShowCameraScrollSpeed,
// EESETSETMediaSendMessage, EESETSETMiniMapFlare, EESETSETModifyAIMilitaryTarget, EESETSETNoEffect).
// 0x186e1f..0x18849c. After EESETSETMedia.cpp, before EESETSETObject.cpp. Unit flags: the exe module set; include
// set copied from EESETSETAI.cpp (inferred).
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
#include "ee/EESEARust.h"
void FillAreaObjectComboBox(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, bool inAreas);   // EESETSETPlayer.cpp
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
#include "ee/EESETSETMedia.h"
#include "ee/EESETSETPlayer.h"
#include "ee/EESETSETNoEffect.h"
#include "ee/EEStringIDs.h"
void FillScenarioComboBox(EESETSEScenarioComboBox* inComboBox, UITextLabel* inLabel, bool inAreas);   // 0x16fe85 (EESEEvents.cpp)

// FUNCTION: EXE 0x586e1f
// Shows the send-dialogue controls: player, sound file, message, camera scroll speed slider (0..11), duration, object
void EESETSETMediaSendDialog::Show(EESETSEffectControls* inControls)
{
    std::vector<UWideString> theNames;
    GERectangle theRect;
    UWideString theText;
    UWideString theMoreText;
    GE2DPoint theLocation;
    EESEERust* theEvent;
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[2]->GetRectangle().GetBottom() + 3.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0x8a);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(220.0f);
    theLocation.SetY(690.0f);
    gResource->GetString(IDS_FILE_NAME, theText);
    gResource->GetString(IDS_INCLUDE_MP3_OR_WAV_EXTENSION, theMoreText);
    theText += theMoreText;
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[5]->GetRectangle().GetBottom() + 2.0f);
    inControls->mTextEditBox->ChangeWidth(0xaf);
    inControls->mTextEditBox->SetText(UWideString(""));
    inControls->mTextEditBox->SetLocation(theLocation);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_MESSAGE, theText);
    theLocation.SetX(inControls->mPlayers[0]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 225.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEditBoxes2[1]->SetLocation(theLocation);
    inControls->mEditBoxes2[1]->SetText(UWideString(""));
    inControls->mEditBoxes2[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[4]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 160.0f, theLocation.GetY() + 20.0f));
    gResource->GetString(IDS_CAMERA_SCROLL_SPEED, theText);
    inControls->mLabels[7]->SetRectangle(theRect);
    inControls->mLabels[7]->SetText(theText);
    inControls->mLabels[7]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[7]->GetRectangle().GetLeft());
    theLocation.SetY(inControls->mLabels[7]->GetRectangle().GetBottom() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 110.0f, theLocation.GetY() + 20.0f));
    inControls->mSliders[0]->SetRectangle(theRect);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeShown);
    theEvent = EESEENewObjects(kSEEMSDCameraScroll, this, inControls);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    inControls->mSliders[0]->SetRange(0, 0xb);
    inControls->mSliders[0]->SetCurrentRangeValue(0);
    theLocation.SetX(inControls->mSliders[0]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 49.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[8]->SetRectangle(theRect);
    inControls->mLabels[8]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[8]->SetAlignment(kTextAlignRight);
    inControls->mLabels[8]->SetText(inControls->mSliders[0]->GetCurrentRangeValue());
    gResource->GetString(IDS_DURATION_IN_SECONDS, theText);
    theLocation.SetX(inControls->mLabels[7]->GetRectangle().GetLeft());
    theLocation.SetY(inControls->mLabels[8]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 160.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 1.0f);
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
    theLocation.SetX(10.0f);
    theLocation.SetY(690.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[6]->SetRectangle(theRect);
    inControls->mLabels[6]->SetText(theText);
    inControls->mLabels[6]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[6]->GetRectangle().GetBottom() + 2.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(200);
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[1]->mMode = kSETriggerScreenObject;
    gWorld.mScenarioManager->GetObjectNames(theNames);
    for (unsigned long i = 0; i < theNames.size(); i++)
        inControls->mScenarioComboBoxes[1]->AppendLine(theNames[i]);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    inControls->mLabels[8]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeHidden);
}

// SYNTHETIC: EXE 0x587416
// (EESETSETMSDCameraScrollChange's scalar deleting destructor)

// FUNCTION: EXE 0x587441
// Labels the camera scroll speed slider: 'Off' at 0, 'Snap' at 1, else the speed (value - 1)
void EESETSETMediaSendDialog::ShowCameraScrollSpeed(EESETSEffectControls* inControls)
{
    UWideString theText;
    long theValue = inControls->mSliders[0]->GetCurrentRangeValue();
    if (theValue == 0) {
        gResource->GetString(IDS_GAME_OFF, theText);
        inControls->mLabels[8]->SetText(theText);
    } else if (theValue == 1) {
        gResource->GetString(IDS_SNAP, theText);
        inControls->mLabels[8]->SetText(theText);
    } else {
        inControls->mLabels[8]->SetText(theValue - 1);
    }
}

// FUNCTION: EXE 0x5874b3
// Stores the chat message's player, duration and text into the effect
void EESETSETMediaSendMessage::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    inEffect->mMessagePlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    inEffect->mDuration = inControls->mEditBoxes[0]->GetTextAsInt(0);
    inControls->mEditBoxes2[0]->GetAllText(theText);
    inEffect->mMessage = theText;
}

// FUNCTION: EXE 0x58751a
// Hides the send-chat controls (player label and picker, duration and message labels and boxes)
void EESETSETMediaSendMessage::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes2[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58755d
// Selects the effect's player without firing the picker event and fills the duration and message boxes
void EESETSETMediaSendMessage::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mPlayers[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mMessagePlayer);
    if (theAction) {
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    inControls->mEditBoxes[0]->SetText(inEffect->mDuration);
    inControls->mEditBoxes2[0]->SetText(inEffect->mMessage);
}

// FUNCTION: EXE 0x5875d7
// Shows the player picker, a duration edit box (0) and an empty message box of the send-chat action
void EESETSETMediaSendMessage::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[2]->GetRectangle().GetBottom() + 3.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0x8a);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_DURATION_IN_SECONDS, theText);
    theLocation.SetY(inControls->mPlayers[0]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_MESSAGE, theText);
    theLocation.SetX(inControls->mPlayers[0]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 250.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEditBoxes2[0]->SetText(UWideString(""));
    inControls->mEditBoxes2[0]->SetLocation(theLocation);
    inControls->mEditBoxes2[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58780d
// Stores the flare's area or object (by the use-area radio) and detaches the radios' event
void EESETSETMiniMapFlare::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inEffect->mUsesLocationArea = inControls->mRadioButtons[0]->GetIsChecked();
    if (inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK) {
        if (inEffect->mUsesLocationArea)
            inEffect->mLocationArea = theSelection;
        else
            inEffect->mLocationObject = theSelection;
    }
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x587871
// Hides the flare's area/object label, combo and radios, detaches their event and shows control 8 again
void EESETSETMiniMapFlare::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mScenarioComboBoxes[1]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeShown);
}

// NEAR: the original keeps inEffect in ebx and theAction in the dead inControls slot, ours hoists GetTriggerEvent into ebx
// FUNCTION: EXE 0x5878e2
// Selects the effect's player, checks area or object, fills the combo with areas or objects and picks the stored one
void EESETSETMiniMapFlare::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
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
    if (inEffect->mUsesLocationArea) {
        inControls->mRadioButtons[0]->Check();
        FillAreaObjectComboBox(inControls->mLabels[3], inControls->mScenarioComboBoxes[1], true);
        theAction = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theAction)
            theAction->Reference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mLocationArea);
    } else {
        inControls->mRadioButtons[1]->Check();
        FillAreaObjectComboBox(inControls->mLabels[3], inControls->mScenarioComboBoxes[1], false);
        theAction = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theAction)
            theAction->Reference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mLocationObject);
    }
    if (theAction) {
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    EESEERust* theEvent = new EESEERust(kSEEUseAreaObject);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
}

// SYNTHETIC: EXE 0x587a34
// (EESEEUseAreaObject's scalar deleting destructor)

// FUNCTION: EXE 0x587a5f
// Shows the use-area / use-object radios and an empty area/object combo, moves the player picker and hides control 8
void EESETSETMiniMapFlare::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation(430.0f, 641.0f);
    GEText* theGEText;
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
    theLocation.SetXY(inControls->mRadioButtons[1]->GetRectangle().GetRight() + 10.0f, 641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 18.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(UWideString("----"));
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 1.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(0xaf);
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[0]->SetFlags(0x88);
    theLocation.SetXY(220.0f, 641.0f);
    inControls->mLabels[2]->SetLocation(theLocation);
    theLocation.SetY(666.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0x88);
    inControls->mScenarioComboBoxes[1]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x587ce2
// Stores the AI target area or object, add or remove, and attack or defend, then detaches the area/object radio event
void EESETSETModifyAIMilitaryTarget::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection);
    if (inControls->mRadioButtons[2]->GetIsChecked() == true) {
        inEffect->mUsesTargetArea = true;
        inEffect->mTargetArea = theSelection;
    } else {
        inEffect->mUsesTargetArea = false;
        inEffect->mTargetObject = theSelection;
    }
    inEffect->mIsAddTarget = inControls->mRadioButtons[0]->GetIsChecked();
    inEffect->mIsAttack = inControls->mRadioButtons[4]->GetIsChecked();
    inControls->mRadioButtons[2]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[3]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x587d6c
// Hides the AI target radios (add/remove, area/object, attack/defend), target combo and label
void EESETSETModifyAIMilitaryTarget::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[2]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[3]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x587df6
// Checks area or object and fills the target combo with the stored target, then checks add/remove and attack/defend
void EESETSETModifyAIMilitaryTarget::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    if (inEffect->mUsesTargetArea) {
        inControls->mRadioButtons[2]->Check();
        FillScenarioComboBox(inControls->mScenarioComboBoxes[1], inControls->mLabels[3], true);
        inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mTargetArea);
        inControls->mScenarioComboBoxes[1]->mMode = kSETriggerScreenArea;
    } else {
        inControls->mRadioButtons[3]->Check();
        FillScenarioComboBox(inControls->mScenarioComboBoxes[1], inControls->mLabels[3], false);
        inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mTargetObject);
        inControls->mScenarioComboBoxes[1]->mMode = kSETriggerScreenObject;
    }
    if (theAction) {
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    if (inEffect->mIsAddTarget)
        inControls->mRadioButtons[0]->Check();
    else
        inControls->mRadioButtons[1]->Check();
    if (inEffect->mIsAttack == true)
        inControls->mRadioButtons[4]->Check();
    else
        inControls->mRadioButtons[5]->Check();
    EESEERust* theEvent = EESEENewUseObjectArea(inControls->mLabels[3], inControls->mScenarioComboBoxes[1],
                                                inControls->mRadioButtons[2]);
    inControls->mRadioButtons[2]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[3]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
}

// FUNCTION: EXE 0x587f71
// Shows the add/remove, attack/defend and use-area/use-object radios and an empty target combo
void EESETSETModifyAIMilitaryTarget::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    GEText* theGEText;
    gResource->GetString(IDS_TRIGGER_EDITOR_ADD, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(641.0f);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[0]->SetLocation(theLocation);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SE_REMOVE, theText);
    theLocation.SetY(inControls->mRadioButtons[0]->GetRectangle().GetBottom() + 1.0f);
    inControls->mRadioButtons[1]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[1]->SetLocation(theLocation);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mRadioButtons[1]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(641.0f);
    gResource->GetString(IDS_SE_ATTACK, theText);
    inControls->mRadioButtons[4]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[4]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[4]->SetLocation(theLocation);
    inControls->mRadioButtons[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mRadioButtons[4]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_DEFEND, theText);
    inControls->mRadioButtons[5]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[5]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[5]->SetLocation(theLocation);
    inControls->mRadioButtons[5]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(270.0f);
    theLocation.SetY(inControls->mPlayers[0]->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_USE_AREA, theText);
    inControls->mRadioButtons[2]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[2]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[2]->SetLocation(theLocation);
    inControls->mRadioButtons[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mRadioButtons[2]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, theText);
    inControls->mRadioButtons[3]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[3]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[3]->SetLocation(theLocation);
    inControls->mRadioButtons[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(405.0f);
    theLocation.SetY(inControls->mPlayers[0]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(UWideString("----"));
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mScenarioComboBoxes[1]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[1]->ChangeWidth(0xaf);
    inControls->mScenarioComboBoxes[1]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x588399
// Shows the effect title, effect type combo and the trigger's name, duplicate and delete controls again
void EESETSETNoEffect::VirtualC(EESETSEffectControls* inControls)
{
    UIForm* theForm = inControls->mTextEditBox->GetForm();
    theForm->GetControlById(kSETSEffectTitleLabel)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETSEffectTypeComboBox)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenNameEditBox)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenNameLabel)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenDuplicateButton)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenDeleteButton)->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x58841b
// Hides the effect title, effect type combo and the trigger's name, duplicate and delete controls
void EESETSETNoEffect::Show(EESETSEffectControls* inControls)
{
    UIForm* theForm = inControls->mTextEditBox->GetForm();
    theForm->GetControlById(kSETSEffectTitleLabel)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETSEffectTypeComboBox)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenNameEditBox)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenNameLabel)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenDuplicateButton)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenDeleteButton)->SetDisplayMode(kDisplayModeHidden);
}
