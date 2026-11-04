// C:\EEX\Empire Earth\Scenario Editor\EESETSConditionType.cpp (file name inferred: no __FILE__; the RTTI classes
// EESETSConditionType and its seven EESETSCT* types, ee/EESETSConditionType.h). The condition types' slots:
// 0x17ef10..0x181792, by class (AlwaysTrue, ChatMessage, Diplomacy, Game, PlayerResearch, PlayerResource,
// TriggerAttribute), with the COMDATs their bodies first use (EESEE event ctors/??_G, vector<long> members). After
// EESETSCondition.cpp (..0x17ef10, its type ??_G), before EESETSEComboBox.cpp (the effect controls' ctor 0x181792).
// Unit flags: the exe module set.
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

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESMCondition.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EETileLocation.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSCComboBox.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESEEConditionPlayerResource.h"
#include "ee/EESETSConditionType.h"
#include "ee/EESEEvents.h"
#include "LostDebug.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x57ef10
// Shows the condition title and type combo box and the trigger tab's name, duplicate and delete controls again
void EESETSCTAlwaysTrue::Hide(EESETSConditionControls* inControls)
{
    UIForm* theForm = inControls->mEditBoxes[0]->GetForm();
    theForm->GetControlById(kSETSConditionTitleLabel)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETSConditionTypeComboBox)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenNameEditBox)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenNameLabel)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenDuplicateButton)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControlById(kSETriggerScreenDeleteButton)->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x57ef92
// Hides the condition title and type combo box and the trigger tab's name, duplicate and delete controls
void EESETSCTAlwaysTrue::Show(EESETSConditionControls* inControls)
{
    UIForm* theForm = inControls->mEditBoxes[0]->GetForm();
    theForm->GetControlById(kSETSConditionTitleLabel)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETSConditionTypeComboBox)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenNameEditBox)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenNameLabel)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenDuplicateButton)->SetDisplayMode(kDisplayModeHidden);
    theForm->GetControlById(kSETriggerScreenDeleteButton)->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57f013
// Writes the chat text from the edit box into the condition
void EESETSCTChatMessage::Store(EESMCondition* outCondition, EESETSConditionControls* inControls)
{
    UWideString theText(inControls->mTextEditBox->GetText(0));
    outCondition->mChatText = theText;
}

// FUNCTION: EXE 0x57f04e
// Hides the chat text edit box and its label
void EESETSCTChatMessage::Hide(EESETSConditionControls* inControls)
{
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57f06a
// Shows the condition's chat text in the edit box
void EESETSCTChatMessage::Load(EESMCondition* inCondition, EESETSConditionControls* inControls)
{
    inControls->mTextEditBox->SetText(inCondition->mChatText);
}

// FUNCTION: EXE 0x57f082
// Shows the chat label and an empty chat text edit box (Enter stores the entry)
void EESETSCTChatMessage::Show(EESETSConditionControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    UWideString theEmpty;
    gResource->GetString(IDS_GAME_CHAT, theText);
    theRect = GERectangle(4.0f, 616.0f, 304.0f, 636.0f);
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    theRect = GERectangle(14.0f, 641.0f, 304.0f, 661.0f);
    inControls->mTextEditBox->SetText(theEmpty);
    inControls->mTextEditBox->SetRectangle(theRect);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeShown);
    UIEvent* theEvent = new EESEESelectName;
    inControls->mTextEditBox->SetTriggerEvent(kControlTriggerEnterPressed, theEvent);
    theEvent->Dereference();
}

// FUNCTION: EXE 0x57f18f
// Writes the two players and the diplomacy state picked in the controls into the condition
void EESETSCTDiplomacy::Store(EESMCondition* outCondition, EESETSConditionControls* inControls)
{
    outCondition->mPlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    outCondition->mOtherPlayer = inControls->mPlayers[1]->GetSelectedPlayer();
    inControls->mComboBoxes[0]->GetCurrentSelection(outCondition->mDiplomacy);
}

// FUNCTION: EXE 0x57f1c0
// Hides the diplomacy controls and resets the 'towards' and 'is' labels to left alignment
void EESETSCTDiplomacy::Hide(EESETSConditionControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetAlignment(kTextAlignLeft);
    inControls->mPlayers[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetAlignment(kTextAlignLeft);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57f21d
// Shows the condition's two players and diplomacy state in the controls
void EESETSCTDiplomacy::Load(EESMCondition* inCondition, EESETSConditionControls* inControls)
{
    inControls->mPlayers[0]->SetSelectedPlayer(inCondition->mPlayer);
    inControls->mPlayers[1]->SetSelectedPlayer(inCondition->mOtherPlayer);
    inControls->mComboBoxes[0]->SetCurrentSelection(inCondition->mDiplomacy);
}

// FUNCTION: EXE 0x57f24e
// Lays out the diplomacy condition: title, player, 'towards' player, 'is' and the allied..enemy combo box
void EESETSCTDiplomacy::Show(EESETSConditionControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_DIPLOMACY_CONDITION_DETAILS, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 300.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 8.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 150.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 30.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0xde);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_DIPLOMACY_TOWARDS, theText);
    theLocation.SetX(inControls->mPlayers[0]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 8.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 323.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[1]->SetRectangle(theRect);
    inControls->mLabels[1]->SetText(theText);
    inControls->mLabels[1]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mPlayers[0]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 30.0f);
    inControls->mPlayers[1]->SetLocation(theLocation);
    inControls->mPlayers[1]->SetFlags(0xde);
    inControls->mPlayers[1]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_IS, theText);
    theLocation.SetX(inControls->mPlayers[1]->GetRectangle().GetRight() + 2.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 20.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[2]->GetRectangle().GetRight() + 2.0f);
    inControls->mComboBoxes[0]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_ALLIED; theIndex <= IDS_TRIGGER_ENEMY; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->ChangeWidth(0x96);
    inControls->mComboBoxes[0]->SetLocation(theLocation);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x57f558
// Writes the difficulty, elapsed time window and hour tests into the condition; a minimum above the maximum
// is lowered to the maximum (the earlier raise of the maximum is overwritten at once)
void EESETSCTGame::Store(EESMCondition* outCondition, EESETSConditionControls* inControls)
{
    outCondition->mUsesDifficulty = inControls->mCheckBoxes[0]->GetIsChecked();
    inControls->mComboBoxes[0]->GetCurrentSelection(outCondition->mDifficulty);
    outCondition->mUsesElapsedTime = inControls->mCheckBoxes[1]->GetIsChecked();
    outCondition->mMinTime = inControls->mEditBoxes[0]->GetTextAsInt(0);
    if (outCondition->mMinTime > outCondition->mMaxTime)
        outCondition->mMaxTime = outCondition->mMinTime;
    outCondition->mMaxTime = inControls->mEditBoxes[1]->GetTextAsInt(0);
    if (outCondition->mMaxTime < outCondition->mMinTime)
        outCondition->mMinTime = outCondition->mMaxTime;
    outCondition->mUsesHour = inControls->mCheckBoxes[2]->GetIsChecked();
    outCondition->mHour = inControls->mSlider->GetCurrentRangeValue();
    EE_LOST_DEBUG(1);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x57f5d8
// Hides the game condition's controls and detaches the check box and slider events
void EESETSCTGame::Hide(EESETSConditionControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[1]->SetAlignment(kTextAlignLeft);
    inControls->mCheckBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSlider->SetDisplayMode(kDisplayModeHidden);
    inControls->mSlider->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[2]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mCheckBoxes[2]->SetTriggerEvent(kControlTriggerUnchecked, NULL);
    inControls->mCheckBoxes[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mCheckBoxes[0]->SetTriggerEvent(kControlTriggerUnchecked, NULL);
    inControls->mCheckBoxes[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mCheckBoxes[1]->SetTriggerEvent(kControlTriggerUnchecked, NULL);
}

// FUNCTION: EXE 0x57f6a9
// Shows the condition's difficulty, elapsed time window and hour settings in the controls
void EESETSCTGame::Load(EESMCondition* inCondition, EESETSConditionControls* inControls)
{
    inControls->mCheckBoxes[0]->SetChecked(inCondition->mUsesDifficulty);
    inControls->mComboBoxes[0]->SetCurrentSelection(inCondition->mDifficulty);
    inControls->mCheckBoxes[1]->SetChecked(inCondition->mUsesElapsedTime);
    inControls->mEditBoxes[0]->SetText(inCondition->mMinTime);
    inControls->mEditBoxes[1]->SetText(inCondition->mMaxTime);
    inControls->mCheckBoxes[2]->SetChecked(inCondition->mUsesHour);
    inControls->mSlider->SetCurrentRangeValue(inCondition->mHour);
}

// FUNCTION: EXE 0x57f71a
// Lays out the game condition: difficulty check box and combo, elapsed time min/max boxes, hour check box
// and 0..23 slider; each check box shows or hides its fields
void EESETSCTGame::Show(EESETSConditionControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_GAME_CONDITION_DETAILS, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 300.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_DIFFICULTY_LEVEL_AT, theText);
    theLocation.SetX(inControls->mLabels[0]->GetRectangle().GetLeft() + 10.0f);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 10.0f);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->ChangeWidth(200);
    inControls->mCheckBoxes[0]->SetText(theText, gU531934);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
    EESEECheckDisplay* theEvent = new EESEECheckDisplay;
    inControls->mCheckBoxes[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mCheckBoxes[0]->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->mControls.push_back(inControls->mComboBoxes[0]);
    theEvent->Dereference();
    GEText* theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);

    theLocation.SetY(inControls->mCheckBoxes[0]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[0]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_EASY; theIndex <= IDS_TRIGGER_HARD; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->ChangeWidth(200);
    inControls->mComboBoxes[0]->SetLocation(theLocation);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_ELAPSED_TIME_IS, theText);
    theLocation.SetX(inControls->mComboBoxes[0]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(inControls->mCheckBoxes[0]->GetRectangle().GetTop());
    inControls->mCheckBoxes[1]->SetLocation(theLocation);
    inControls->mCheckBoxes[1]->ChangeWidth(200);
    inControls->mCheckBoxes[1]->SetText(theText, gU531934);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theEvent = new EESEECheckDisplay;
    inControls->mCheckBoxes[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mCheckBoxes[1]->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->mControls.push_back(inControls->mLabels[1]);
    theEvent->mControls.push_back(inControls->mEditBoxes[0]);
    theEvent->mControls.push_back(inControls->mLabels[2]);
    theEvent->mControls.push_back(inControls->mEditBoxes[1]);
    theEvent->Dereference();
    theGEText = inControls->mCheckBoxes[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);

    gResource->GetString(IDS_TRIGGER_EDITOR_MIN, theText);
    theLocation.SetX(inControls->mCheckBoxes[1]->GetRectangle().GetLeft() + 40.0f);
    theLocation.SetY(inControls->mCheckBoxes[1]->GetRectangle().GetBottom() + 3.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 60.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[1]->SetRectangle(theRect);
    inControls->mLabels[1]->SetText(theText);
    inControls->mLabels[1]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[1]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_TRIGGER_EDITOR_MAX, theText);
    theLocation.SetX(inControls->mLabels[1]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(inControls->mLabels[1]->GetRectangle().GetTop());
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 60.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[2]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEditBoxes[1]->SetLocation(theLocation);
    inControls->mEditBoxes[1]->SetText((unsigned long)0);
    inControls->mEditBoxes[1]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_HOUR_IS_AT_LEAST, theText);
    theLocation.SetX(inControls->mCheckBoxes[1]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(inControls->mCheckBoxes[0]->GetRectangle().GetTop());
    inControls->mCheckBoxes[2]->SetLocation(theLocation);
    inControls->mCheckBoxes[2]->ChangeWidth(200);
    inControls->mCheckBoxes[2]->SetText(theText, gU531934);
    inControls->mCheckBoxes[2]->SetDisplayMode(kDisplayModeShown);
    theEvent = new EESEECheckDisplay;
    inControls->mCheckBoxes[2]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mCheckBoxes[2]->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->mControls.push_back(inControls->mSlider);
    theEvent->mControls.push_back(inControls->mLabels[3]);
    theEvent->Dereference();
    theGEText = inControls->mCheckBoxes[2]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);

    theLocation.SetX(inControls->mCheckBoxes[2]->GetRectangle().GetLeft() + 60.0f);
    theLocation.SetY(inControls->mCheckBoxes[2]->GetRectangle().GetBottom() + 3.0f);
    inControls->mSlider->SetRange(0, 0x17);
    inControls->mSlider->SetLocation(theLocation);
    inControls->mSlider->SetPageSize(1);
    inControls->mSlider->SetDisplayMode(kDisplayModeShown);
    UIEvent* theSliderEvent = new EESEESliderChange(inControls->mLabels[3], UWideString("%2.0f"), 1.0f, 0.0f);
    inControls->mSlider->SetTriggerEvent(kControlTriggerRangeChanged, theSliderEvent);
    theSliderEvent->Dereference();
    theLocation.SetX(inControls->mSlider->GetRectangle().GetRight() + 2.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 20.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(inControls->mSlider->GetCurrentRangeValue());
    inControls->mLabels[3]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x57fdf1
// Creates a slider event that prints the value times a scale plus an offset into a label (EESEESliderChange's ctor)

// SYNTHETIC: EXE 0x57fe51
// (the scalar deleting destructor, emitted with the vtable; slot EESEESliderChange+0x0, stores EESEESliderChange vtable)

// FUNCTION: EXE 0x57fe85
// Creates a check box event with an empty list of controls to show or hide (EESEECheckDisplay's ctor)

// SYNTHETIC: EXE 0x57febd
// (the scalar deleting destructor, emitted with the vtable; slot EESEECheckDisplay+0x0, stores EESEECheckDisplay vtable)

// FUNCTION: EXE 0x57fef0
// Creates an empty vector<UIControl*> (the check box event's control list; vector ctor)

// FUNCTION: EXE 0x57ff06
// Appends a control to a vector<UIControl*> (vector::push_back)

// FUNCTION: EXE 0x57ff32
// Inserts n copies of a value into a vector<long> (vector<long>::insert; the address is also bound to
// vector<EEAreaEffectTemplate*>::insert)

// FUNCTION: EXE 0x5801c8
// Writes the player and the technology picked in the research combo box into the condition
void EESETSCTPlayerResearch::Store(EESMCondition* outCondition, EESETSConditionControls* inControls)
{
    unsigned long theSelection;
    outCondition->mPlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    inControls->mComboBoxes[0]->GetCurrentSelection(theSelection);
    outCondition->mTechnology = this->mTechnologies[theSelection];
}

// FUNCTION: EXE 0x5801f6
// Hides the research condition's labels, player combo and technology combo
void EESETSCTPlayerResearch::Hide(EESETSConditionControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x580226
// Shows the condition's player and technology (the first entry when the technology is not in the list)
void EESETSCTPlayerResearch::Load(EESMCondition* inCondition, EESETSConditionControls* inControls)
{
    inControls->mPlayers[0]->SetSelectedPlayer(inCondition->mPlayer);
    long theTech = inCondition->mTechnology;
    std::vector<long>::iterator theIt = std::find(this->mTechnologies.begin(), this->mTechnologies.end(), theTech);
    if (theIt != this->mTechnologies.end())
        inControls->mComboBoxes[0]->SetCurrentSelection(theIt - this->mTechnologies.begin());
    else
        inControls->mComboBoxes[0]->SetCurrentSelection(0);
}

// the research combo box sorts its technologies by name (std::sort's predicate; its operator() is the COMDAT
// 0x180855, after the sort helpers) (provisional name)
struct EESETSCTResearchNameLess {
    bool operator()(const long& inLeft, const long& inRight) const
    {
        UWideString theLeft;
        UWideString theRight;
        EETechTreeNode* theNode;
        theNode = gTechTree.Get(inLeft);
        if (theNode) {
            RTSTechTreeNodeData* theData = theNode->mData;
            if (theData->mUnitClass >= 0 && gUnitClassTable[theData->mUnitClass])
                gResource->GetString(gUnitClassTable[theData->mUnitClass]->mNameStringID, theLeft);
            else
                gResource->GetString(theData->mNameID, theLeft);
        } else
            theLeft = "";
        theNode = gTechTree.Get(inRight);
        if (theNode) {
            RTSTechTreeNodeData* theData = theNode->mData;
            if (theData->mUnitClass >= 0 && gUnitClassTable[theData->mUnitClass])
                gResource->GetString(gUnitClassTable[theData->mUnitClass]->mNameStringID, theRight);
            else
                gResource->GetString(theData->mNameID, theRight);
        } else
            theRight = "";
        return theLeft < theRight;
    }
};

// FUNCTION: EXE 0x5802a4
// Lays out the 'player has researched' condition; the first call for this object lists the editor-visible, non-unit
// technologies and ids 0..14 sorted by name
void EESETSCTPlayerResearch::Show(EESETSConditionControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    // Built on the first Show of this type object, which is recreated with every scenario editor (a run-wide
    // flag left a later editor session with an empty list).
    if (this->mTechnologies.empty()) {
        gTechTree.GetNodeIDs(this->mTechnologies);
        for (unsigned long theEpoch = 0; theEpoch < 15; theEpoch++)
            this->mTechnologies.push_back((long)theEpoch);
        unsigned long theIndex = 0;
        while (theIndex < this->mTechnologies.size()) {
            if (gTechTree.Get(this->mTechnologies[theIndex])->mData->mShowInEditor == true &&
                gTechTree.Get(this->mTechnologies[theIndex])->mData->mMakesUnit == false)
                theIndex++;
            else
                this->mTechnologies.erase(this->mTechnologies.begin() + theIndex);
        }
        std::sort(this->mTechnologies.begin(), this->mTechnologies.end(), EESETSCTResearchNameLess());
    }
    gResource->GetString(IDS_PLAYER_CONDITION_DETAILS, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 300.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 10.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0xde);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_HAS_RESEARCHED, theText);
    theLocation.SetX(inControls->mPlayers[0]->GetRectangle().GetRight() + 10.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 200.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[1]->SetRectangle(theRect);
    inControls->mLabels[1]->SetText(theText);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[1]->GetRectangle().GetBottom() + 10.0f);
    inControls->mComboBoxes[0]->DeleteAllEntries();
    for (unsigned long i = 0; i < this->mTechnologies.size(); i++) {
        EETechTreeNode* theNode = gTechTree.Get(this->mTechnologies[i]);
        if (theNode) {
            RTSTechTreeNodeData* theData = theNode->mData;
            if (theData->mUnitClass >= 0 && gUnitClassTable[theData->mUnitClass])
                gResource->GetString(gUnitClassTable[theData->mUnitClass]->mNameStringID, theText);
            else
                gResource->GetString(theData->mNameID, theText);
        } else
            theText = "";
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->ChangeWidth(0xfa);
    inControls->mComboBoxes[0]->SetLocation(theLocation);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x5805c0
// Sorts technology ids by display name (std::sort with EESETSCTResearchNameLess)

// FUNCTION: EXE 0x58067d
// Insertion-sorts a range of technology ids by name (std::_Insertion_sort)

// FUNCTION: EXE 0x5806a6
// Quick-sorts technology ids by name, falling back to heap sort when too deep (std::_Sort)

// FUNCTION: EXE 0x580779
// Insertion-sorts the rest of a range without a lower bound check (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x5807aa
// Insertion-sorts a range of technology ids by name (std::_Insertion_sort_1)

// FUNCTION: EXE 0x580855
// Compares two technology ids by display name: the unit class name for unit-making nodes, else the
// node's name; unknown ids count as empty (EESETSCTResearchNameLess::operator())
// (inline, defined above 0x5802a4)

// FUNCTION: EXE 0x580942
// Arranges a range of technology ids into a heap by name (std::make_heap)

// FUNCTION: EXE 0x58097c
// Sorts a heap of technology ids by name (std::sort_heap)

// FUNCTION: EXE 0x5809bd
// Partitions technology ids around a pivot name (std::_Unguarded_partition)

// FUNCTION: EXE 0x580ace
// Moves one technology id down into its sorted place (std::_Unguarded_insert)

// FUNCTION: EXE 0x580afe
// Builds a heap of technology ids by name (std::_Make_heap)

// FUNCTION: EXE 0x580b42
// Moves the largest technology id of a heap to its end (std::pop_heap)

// FUNCTION: EXE 0x580b6b
// Sifts a technology id down a heap by name (std::_Adjust_heap)

// FUNCTION: EXE 0x580bd8
// Pops the top technology id of a heap (std::_Pop_heap_0)

// FUNCTION: EXE 0x580c2b
// Sifts a technology id up a heap by name (std::_Push_heap)

// FUNCTION: EXE 0x580c7c
// Pops the top technology id of a heap into a given slot (std::_Pop_heap)

// FUNCTION: EXE 0x580cb0
// Writes the player, attribute, comparison, amount, other player and attribute, and whether to compare
// with that attribute, into the condition
void EESETSCTPlayerResource::Store(EESMCondition* outCondition, EESETSConditionControls* inControls)
{
    outCondition->mPlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    inControls->mComboBoxes[0]->GetCurrentSelection(outCondition->mAttribute);
    inControls->mComboBoxes[1]->GetCurrentSelection(outCondition->mOperator);
    outCondition->mAmount = inControls->mEditBoxes[0]->GetTextAsInt(0);
    outCondition->mOtherPlayer = inControls->mPlayers[1]->GetSelectedPlayer();
    inControls->mComboBoxes[2]->GetCurrentSelection(outCondition->mOtherAttribute);
    outCondition->mUsesPlayerAttribute = inControls->mRadioButtons[1]->GetIsChecked();
    EE_LOST_DEBUG(1);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x580d16
// Hides the player attribute condition's controls and detaches the radio buttons' event
void EESETSCTPlayerResource::Hide(EESETSConditionControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x580d99
// Shows the condition's attribute test in the controls and checks the amount or player-attribute radio button
void EESETSCTPlayerResource::Load(EESMCondition* inCondition, EESETSConditionControls* inControls)
{
    inControls->mPlayers[0]->SetSelectedPlayer(inCondition->mPlayer);
    inControls->mComboBoxes[0]->SetCurrentSelection(inCondition->mAttribute);
    inControls->mComboBoxes[1]->SetCurrentSelection(inCondition->mOperator);
    inControls->mEditBoxes[0]->SetText(inCondition->mAmount);
    inControls->mPlayers[1]->SetSelectedPlayer(inCondition->mOtherPlayer);
    inControls->mComboBoxes[2]->SetCurrentSelection(inCondition->mOtherAttribute);
    UIRadioButton* theButton;
    if (inCondition->mUsesPlayerAttribute)
        theButton = inControls->mRadioButtons[1];
    else
        theButton = inControls->mRadioButtons[0];
    theButton->Check();
    this->mEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theButton);
}

// FUNCTION: EXE 0x580e23
// Lays out the player attribute condition: player, attribute (resources, variables, sent/received, stats,
// settings), comparison, and an amount box or another player's attribute picked by radio buttons
void EESETSCTPlayerResource::Show(EESETSConditionControls* inControls)
{
    GERectangle theRectangle;
    std::vector<UWideString> theResources;
    UWideString theText;
    UWideString theFormatted;
    UWideString theSuffix;
    GE2DPoint theLocation;

    gResource->GetString(IDS_PLAYER_ATTRIBUTE_CONDITION_DETAILS, theText);
    theLocation.SetXY(4.0f, 616.0f);
    theRectangle = GERectangle(theLocation, GE2DPoint(304.0f, 636.0f));
    inControls->mLabels[0]->SetRectangle(theRectangle);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[0]->GetRectangle().GetBottom() + 10.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0xde);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_S, theText);
    theLocation.SetX(inControls->mPlayers[0]->GetRectangle().GetRight() + 2.0f);
    theRectangle = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 10.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRectangle);
    inControls->mLabels[2]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);

    for (unsigned long theIndex = 0; theIndex <= 10; theIndex++) {
        gResource->GetString(IDS_TRIGGER_RESOURCE_FIRST + theIndex, theText);
        theResources.push_back(theText);
    }
    gResource->GetString(IDS_VARIABLE, theText);
    for (unsigned long i = 0; i < 10; i++) {
        sprintf(theFormatted, "%s%d", (const char*)theText, i);
        theResources.push_back(theFormatted);
    }
    for (i = IDS_RESOURCE_NAME_FIRST; i <= IDS_RESOURCE_NAME_IRON; i++) {
        gResource->GetString(i, theText);
        gResource->GetString(IDS_SENT, theFormatted);
        sprintf(theSuffix, "%s %s", (const char*)theText, (const char*)theFormatted);
        theResources.push_back(theSuffix);
        gResource->GetString(IDS_RECEIVED, theFormatted);
        sprintf(theSuffix, "%s %s", (const char*)theText, (const char*)theFormatted);
        theResources.push_back(theSuffix);
    }
    for (i = IDS_BUILDINGS_LOST; i <= IDS_STAT_FOR_FUTURE_USE6; i++) {
        if (i - 0x79e0 != 0x16 && i - 0x79e0 != 0x17)   // two entries of the 0x79e0 block use string 0x79fe
            gResource->GetString(i, theText);
        else
            gResource->GetString(IDS_STAT_FOR_FUTURE_USE6, theText);
        theResources.push_back(theText);
    }
    for (theIndex = 0; theIndex <= 5; theIndex++) {
        gResource->GetString(IDS_TRIGGER_PLAYER_SETTING_FIRST + theIndex, theText);
        theResources.push_back(theText);
    }

    theLocation.SetX(inControls->mLabels[2]->GetRectangle().GetRight() + 2.0f);
    inControls->mComboBoxes[0]->DeleteAllEntries();
    inControls->mComboBoxes[2]->DeleteAllEntries();
    for (i = 0; i < theResources.size(); i++) {
        inControls->mComboBoxes[0]->AppendLine(theResources[i]);
        inControls->mComboBoxes[2]->AppendLine(theResources[i]);
    }
    theResources.clear();
    inControls->mComboBoxes[0]->GetGEText()->SetFont(gU531930);
    inControls->mComboBoxes[0]->GetListBox()->GetGEText()->SetFont(gU531930);
    inControls->mComboBoxes[0]->ChangeWidth(125);
    inControls->mComboBoxes[0]->SetLocation(theLocation);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);

    for (i = 0; i < 5; i++) {
        gResource->GetString(IDS_TRIGGER_COMPARISON_FIRST + i, theText);
        theResources.push_back(theText);
    }
    theLocation.SetX(inControls->mComboBoxes[0]->GetRectangle().GetRight() + 5.0f);
    inControls->mComboBoxes[1]->DeleteAllEntries();
    for (i = 0; i < 5; i++)
        inControls->mComboBoxes[1]->AppendLine(theResources[i]);
    theResources.clear();
    inControls->mComboBoxes[1]->ChangeWidth(50);
    inControls->mComboBoxes[1]->SetLocation(theLocation);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_USE_AMOUNT, theText);
    theLocation.SetX(inControls->mComboBoxes[1]->GetRectangle().GetRight() + 5.0f);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    GEText* theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[0]->SetLocation(theLocation);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeShown);

    gResource->GetString(IDS_USE_PLAYER_ATTRIBUTE, theText);
    theLocation.SetY(inControls->mRadioButtons[0]->GetRectangle().GetBottom() + 5.0f);
    inControls->mRadioButtons[1]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[1]->SetLocation(theLocation);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeShown);

    theLocation.SetX(inControls->mRadioButtons[0]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mRadioButtons[0]->GetRectangle().GetTop());
    inControls->mEditBoxes[0]->SetText(0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    inControls->mPlayers[1]->SetLocation(theLocation);
    inControls->mPlayers[1]->SetFlags(0xde);
    inControls->mPlayers[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mPlayers[1]->GetRectangle().GetBottom() + 5.0f);
    inControls->mComboBoxes[2]->GetGEText()->SetFont(gU531930);
    inControls->mComboBoxes[2]->GetListBox()->GetGEText()->SetFont(gU531930);
    inControls->mComboBoxes[2]->ChangeWidth(125);
    inControls->mComboBoxes[2]->SetLocation(theLocation);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, this->mEvent);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, this->mEvent);
    inControls->mRadioButtons[0]->Check();
}

// FUNCTION: EXE 0x581450
// Writes the picked trigger (when one is picked) and the 'is on' / 'has fired' tests into the condition
void EESETSCTTriggerAttribute::Store(EESMCondition* outCondition, EESETSConditionControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mScenarioComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK)
        outCondition->mTrigger = theSelection;
    outCondition->mIsTriggerOn = inControls->mCheckBoxes[0]->GetIsChecked();
    outCondition->mHasTriggerFired = inControls->mCheckBoxes[1]->GetIsChecked();
}

// FUNCTION: EXE 0x58148a
// Hides the trigger attribute condition's controls
void EESETSCTTriggerAttribute::Hide(EESETSConditionControls* inControls)
{
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x5814c3
// Shows the condition's trigger and its 'is on' / 'has fired' tests in the controls
void EESETSCTTriggerAttribute::Load(EESMCondition* inCondition, EESETSConditionControls* inControls)
{
    inControls->mScenarioComboBoxes[0]->SetCurrentSelection(inCondition->mTrigger);
    inControls->mCheckBoxes[0]->SetChecked(inCondition->mIsTriggerOn);
    inControls->mCheckBoxes[1]->SetChecked(inCondition->mHasTriggerFired);
}

// FUNCTION: EXE 0x581502
// Lays out the trigger attribute condition: title, a combo box of the trigger names, 'is on' and 'has fired'
void EESETSCTTriggerAttribute::Show(EESETSConditionControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TRIGGER_CONDITION_ATTRIBUTES, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 300.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[0]->SetRectangle(theRect);
    inControls->mLabels[0]->SetText(theText);
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_EDITOR_TRIGGER, theText);
    theLocation.SetX(14.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 290.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[1]->SetRectangle(theRect);
    inControls->mLabels[1]->SetText(theText);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mScenarioComboBoxes[0]->DeleteAllEntries();
    gWorld.mScenarioManager->GetTriggerNames(theNames);
    for (unsigned long i = 0; i < theNames.size(); i++)
        inControls->mScenarioComboBoxes[0]->AppendLine(theNames[i]);
    inControls->mScenarioComboBoxes[0]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[0]->ChangeWidth(0x122);
    inControls->mScenarioComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
    inControls->mScenarioComboBoxes[0]->mMode = kSETriggerScreenTrigger;
    theLocation.SetX(inControls->mScenarioComboBoxes[0]->GetRectangle().GetRight() + 10.0f);
    gResource->GetString(IDS_IS_ON, theText);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->ChangeWidth(100);
    inControls->mCheckBoxes[0]->SetText(theText, gU531934);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
    GEText* theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    theLocation.SetY(inControls->mCheckBoxes[0]->GetRectangle().GetBottom() + 10.0f);
    gResource->GetString(IDS_HAS_FIRED, theText);
    inControls->mCheckBoxes[1]->SetLocation(theLocation);
    inControls->mCheckBoxes[1]->ChangeWidth(100);
    inControls->mCheckBoxes[1]->SetText(theText, gU531934);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theGEText = inControls->mCheckBoxes[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
}
