// C:\EEX\Empire Earth\Scenario Editor\EESETSETMedia.cpp (file name inferred: no __FILE__). 0x185fed..0x186e1f:
// the media effect page EESETSETMedia and its four media actions. Unit flags: the exe module set.
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
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSETMedia.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
// FUNCTION: EXE 0x585fed
// Creates the media effect page with its four actions (sound, change text, dialogue, chat) and its combo event
EESETSETMedia::EESETSETMedia()
{
    this->mTypes[kSMMediaActionPlaySound] = new EESETSETMediaPlayGraphic;
    this->mTypes[kSMMediaActionChangeText] = new EESETSETMediaChangeText;
    this->mTypes[kSMMediaActionSendDialogue] = new EESETSETMediaSendDialog;
    this->mTypes[kSMMediaActionSendChatMessage] = new EESETSETMediaSendMessage;
    this->mCurrentType = 0;
    this->mAction = EECreateScreenEvent(EE_SCREENS_SE_CHANGE_EFFECT_ACTION);
}

// SYNTHETIC: EXE 0x58608a
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETMedia+0x0)

// FUNCTION: EXE 0x5860a6
// Destroys the media effect page, deleting its four action types and releasing the action-combo event
EESETSETMedia::~EESETSETMedia()
{
    delete this->mTypes[0];
    delete this->mTypes[1];
    delete this->mTypes[2];
    delete this->mTypes[3];
    this->mAction->Dereference();
}

// FUNCTION: EXE 0x5860f4
// Switches the media effect to the action picked in the combo: hides the old action, shows and fills the new, renames
void EESETSETMedia::ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    inControls->mComboBoxes[0]->GetCurrentSelection(this->mCurrentType);
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
    this->UpdateName(inEffect, inControls);
}

// FUNCTION: EXE 0x586143
// Writes the chosen media action and that action's settings into the effect, then hides the action's controls
void EESETSETMedia::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK) {
        inEffect->mMediaAction = theSelection;
        this->mCurrentType = theSelection;
    }
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    this->mTypes[this->mCurrentType]->Store(inEffect, inControls);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
}

// FUNCTION: EXE 0x586198
// Hides the media effect's title and action label, the action combo and the current action's controls
void EESETSETMedia::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
}

// FUNCTION: EXE 0x5861cf
// Shows a media effect in the editor: selects its action without firing the combo event, shows and fills it
void EESETSETMedia::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->mCurrentType = inEffect->mMediaAction;
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mComboBoxes[0]->SetCurrentSelection(this->mCurrentType);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, this->mAction);
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
}

// FUNCTION: EXE 0x586233
// Shows the 'Media effect' title, the action label and the action combo filled with the four media actions
void EESETSETMedia::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_MEDIA_EFFECT, theText);
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
    for (unsigned long theIndex = IDS_TRIGGER_MEDIA_EFFECT_FIRST; theIndex <= IDS_SEND_CHAT_MESSAGE; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mComboBoxes[0]->AppendLine(theText);
    }
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x586394
// Renames a default-named <...> effect to <first word of the media action name> and shows it in the name box
void EESETSETMedia::UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    UWideString theName;
    theText = inEffect->mName;
    if (theText.GetLength() > 0 && theText[0] == '<') {
        gResource->GetString(this->mCurrentType + IDS_TRIGGER_MEDIA_EFFECT_FIRST, theText);
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

// FUNCTION: EXE 0x5864b3
// Destroys an effect type object (EESETSEffectType's scalar deleting destructor; inline empty dtor)

// FUNCTION: EXE 0x586531
// Stores the chosen text object (hints / objectives / history), whether to clear it, and the line to add
void EESETSETMediaChangeText::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(inEffect->mTextObject);
    inEffect->mIsClearText = inControls->mRadioButtons[0]->GetIsChecked();
    if (!inEffect->mIsClearText) {
        theText = inControls->mTextEditBox->GetText(0);
        inEffect->mDescription = theText;
    }
}

// FUNCTION: EXE 0x58659f
// Hides the change-text controls (text object label and combo, clear / add-line radios, line box), drops events
void EESETSETMediaChangeText::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x58660b
// Selects the effect's text object silently and checks 'Clear' (hiding the text box) or 'Add line' with its text
void EESETSETMediaChangeText::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mTextObject);
    if (theAction) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    if (inEffect->mIsClearText) {
        inControls->mRadioButtons[0]->Check();
        inControls->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    } else {
        inControls->mRadioButtons[1]->Check();
        inControls->mTextEditBox->SetText(inEffect->mDescription);
    }
}

// FUNCTION: EXE 0x5866ab
// Shows the text object combo (hints, objectives, history), the clear / add-line radios and an empty line edit box
void EESETSETMediaChangeText::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TEXT_OBJECT, theText);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 150.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(0x96);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_TRIGGER_HINTS; theIndex <= IDS_TRIGGER_HISTORY; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mEffectComboBoxes[1]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(641.0f);
    gResource->GetString(IDS_CLEAR, theText);
    inControls->mRadioButtons[0]->SetText(theText, gU531934);
    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_SE_CLEAR_ADD_LINE);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    GEText* theGEText = inControls->mRadioButtons[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[0]->SetLocation(theLocation);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mRadioButtons[0]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_ADD_LINE, theText);
    inControls->mRadioButtons[1]->SetText(theText, gU531934);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
    theGEText = inControls->mRadioButtons[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[1]->SetLocation(theLocation);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_DESCRIPTION_76A8, theText);
    theLocation.SetX(inControls->mEffectComboBoxes[1]->GetRectangle().GetLeft());
    theLocation.SetY(inControls->mEffectComboBoxes[1]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 150.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mRadioButtons[0]->GetRectangle().GetLeft());
    inControls->mTextEditBox->ChangeWidth(0xfa);
    inControls->mTextEditBox->SetText(UWideString(""));
    inControls->mTextEditBox->SetLocation(theLocation);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x5869fc
// Stores the typed sound file name into the effect (always: the file-name flag is set by the ctor)
void EESETSETMediaPlayGraphic::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    theText = inControls->mTextEditBox->GetText(0);
    if (this->mUsesFileName)
        inEffect->mFileName = theText;
}

// FUNCTION: EXE 0x586a4d
// Hides the play-sound file name label and edit box
void EESETSETMediaPlayGraphic::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x586a6a
// Shows the effect's sound file name in the edit box
void EESETSETMediaPlayGraphic::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    if (this->mUsesFileName)
        inControls->mTextEditBox->SetText(inEffect->mFileName);
}

// FUNCTION: EXE 0x586a8a
// Shows the 'File name (include .mp3 or .wav)' label and an empty file name edit box of the play-sound action
void EESETSETMediaPlayGraphic::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    UWideString theSuffix;
    GE2DPoint theLocation;
    gResource->GetString(IDS_FILE_NAME, theText);
    if (this->mUsesFileName)
        gResource->GetString(IDS_INCLUDE_MP3_OR_WAV_EXTENSION, theSuffix);
    else
        gResource->GetString(IDS_TRIGGER_EDITOR_USE_OBJECT, theSuffix);
    theText += theSuffix;
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 250.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[2]->SetRectangle(theRect);
    inControls->mLabels[2]->SetText(theText);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mTextEditBox->ChangeWidth(0xfa);
    inControls->mTextEditBox->SetText(UWideString(""));
    inControls->mTextEditBox->SetLocation(theLocation);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x586bb0
// Refreshes the send-dialogue camera scroll speed label when its slider moves (EESETSETMSDCameraScrollChange;
// ee_screens editor_events.rs)

// FUNCTION: EXE 0x586bc0
// Stores the dialogue's object, player, duration, message, sound file and camera scroll speed (slider - 1)
void EESETSETMediaSendDialog::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    unsigned long theSelection;
    if (inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK)
        inEffect->mObject = theSelection;
    inEffect->mMessagePlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    inEffect->mDuration = inControls->mEditBoxes[0]->GetTextAsInt(0);
    inControls->mEditBoxes2[1]->GetAllText(theText);
    inEffect->mMessage = theText;
    theText = inControls->mTextEditBox->GetText(0);
    inEffect->mFileName = theText;
    inEffect->mIntValue1 = inControls->mSliders[0]->GetCurrentRangeValue() - 1;
}

// FUNCTION: EXE 0x586c7e
// Hides every send-dialogue control and shows control 8 of the form again (Show hid it)
void EESETSETMediaSendDialog::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes2[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mTextEditBox->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[6]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[7]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[8]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[8]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x586d1a
// Fills the send-dialogue controls from the effect (player and object selected silently) and refreshes the speed label
void EESETSETMediaSendDialog::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
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
    inControls->mEditBoxes2[1]->SetText(inEffect->mMessage);
    theAction = inControls->mScenarioComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mScenarioComboBoxes[1]->SetCurrentSelection(inEffect->mObject);
    if (theAction) {
        inControls->mScenarioComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    inControls->mTextEditBox->SetText(inEffect->mFileName);
    if (inControls->mSliders[0]->GetCurrentRangeValue() != inEffect->mIntValue1 + 1)
        inControls->mSliders[0]->SetCurrentRangeValue(inEffect->mIntValue1 + 1);
    this->ShowCameraScrollSpeed(inControls);
}
