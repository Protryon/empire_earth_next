// C:\EEX\Empire Earth\Scenario Editor\EESETSETPlayer.cpp (file name inferred: no __FILE__; by the one-.cpp-per-effect-type
// rule this range may really be several units). 0x189904..0x18c72e: EESETSETPlayer (ctor, dtor, its six slots) and its
// player effect types (civ points, diplomacy, resource, technology, victory, view), the cdecl FillAreaObjectComboBox
// 0x18b1b0 with the area/object radio events' ProcessEvents, EESETSETSAAAttributeChange::ProcessEvent and
// EESETSETSetAIAttribute's Virtual08/VirtualC. After EESETSETObject.cpp, before EESETSETSAAAttributeChange.cpp. Unit
// flags: the exe module set; include set copied from EESETSETAI.cpp (inferred).
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
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSETPlayer.h"
#include "ee/EESETSETSAAAttributeChange.h"
#include "ee/EESEEUseAreaObject.h"
#include "ee/EECPBehavior.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UIESliderChange.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
// FUNCTION: EXE 0x589904
// Creates the player effect page with its twelve player action types (victory ... mini-map flare) and its combo event
EESETSETPlayer::EESETSETPlayer()
{
    this->mTypes[kSMPlayerActionVictory] = new EESETSETPlayerVictory;
    this->mTypes[kSMPlayerActionDefeat] = new EESETSETPlayerVictory;
    this->mTypes[kSMPlayerActionDiplomacy] = new EESETSETPlayerDiplomacy;
    this->mTypes[kSMPlayerActionSetAttributeLevel] = new EESETSETPlayerResource;
    this->mTypes[kSMPlayerActionModifyTechTree] = new EESETSETPlayerTechnology;
    this->mTypes[kSMPlayerActionScriptCamera] = new EESETSETPlayerView;
    this->mTypes[kSMPlayerActionAwardCivPoints] = new EESETSETPlayerCivPoint;
    this->mTypes[kSMPlayerActionFadeIn] = new EESETSETPCameraFadeIn;
    this->mTypes[kSMPlayerActionFadeOut] = new EESETSETPCameraFadeOut;
    this->mTypes[kSMPlayerActionVisibilityOn] = new EESETSETPVisibility;
    this->mTypes[kSMPlayerActionVisibilityOff] = new EESETSETPVisibility;
    this->mTypes[kSMPlayerActionMiniMapFlare] = new EESETSETMiniMapFlare;
    this->mCurrentType = kSMPlayerActionVictory;
    this->mAction = new EESEEChangeEffectAction;
}

// SYNTHETIC: EXE 0x589a75
// (the scalar deleting destructor, emitted with the vtable; slot EESETSETPlayer+0x0)

// FUNCTION: EXE 0x589a91
// Destroys the player effect page, deleting its twelve action types and releasing the action-combo event
EESETSETPlayer::~EESETSETPlayer()
{
    delete this->mTypes[0];
    delete this->mTypes[2];
    delete this->mTypes[3];
    delete this->mTypes[4];
    delete this->mTypes[5];
    delete this->mTypes[1];
    delete this->mTypes[6];
    delete this->mTypes[7];
    delete this->mTypes[8];
    delete this->mTypes[9];
    delete this->mTypes[10];
    delete this->mTypes[11];
    this->mAction->Dereference();
}

// FUNCTION: EXE 0x589b40
// Switches the player effect to the action picked in the combo: hides the old type's controls, shows and fills the new
void EESETSETPlayer::ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    inControls->mComboBoxes[0]->GetCurrentSelection(theSelection);
    this->mCurrentType = theSelection;
    this->mTypes[this->mCurrentType]->Show(inControls);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
    this->UpdateName(inEffect, inControls);
}

// FUNCTION: EXE 0x589b8f
// Writes the chosen player action, player and the action's own settings into the effect, then hides the action controls
void EESETSETPlayer::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    if (inControls->mComboBoxes[0]->GetCurrentSelection(theSelection) == kErrorOK) {
        inEffect->mPlayerAction = theSelection;
        this->mCurrentType = theSelection;
    }
    inEffect->mPlayer = inControls->mPlayers[0]->GetSelectedPlayer();
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
    this->mTypes[this->mCurrentType]->Store(inEffect, inControls);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
}

// FUNCTION: EXE 0x589bf2
// Hides the player effect's title, action and player labels, player picker and action combo plus the action's controls
void EESETSETPlayer::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    this->mTypes[this->mCurrentType]->VirtualC(inControls);
}

// FUNCTION: EXE 0x589c3c
// Shows a player effect in the editor: selects its action silently, shows the action, sets the player, fills the action
void EESETSETPlayer::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    this->mCurrentType = inEffect->mPlayerAction;
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mComboBoxes[0]->SetCurrentSelection(this->mCurrentType);
    inControls->mComboBoxes[0]->SetTriggerEvent(kControlTriggerSelectionChanged, this->mAction);
    this->mTypes[this->mCurrentType]->Show(inControls);
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mPlayer);
    this->mTypes[this->mCurrentType]->Load(inEffect, inControls);
}

// FUNCTION: EXE 0x589cae
// Shows the 'Player effect' title, the action label and combo (filled with the player actions) and the player picker
void EESETSETPlayer::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_PLAYER_EFFECT, theText);
    theLocation.SetX(4.0f);
    theLocation.SetY(616.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 210.0f, theLocation.GetY() + 20.0f));
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
    for (unsigned long theIndex = IDS_TRIGGER_PLAYER_EFFECT_FIRST; theIndex <= IDS_MINI_MAP_FLARE; theIndex++) {
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
    inControls->mPlayers[0]->SetFlags(0x80);
    inControls->mPlayers[0]->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x589ebc
// Renames a default-named <...> effect to <action player detail> (other player and stance, attribute, +/- tech, amount)
void EESETSETPlayer::UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UWideString theText;
    UWideString theFormat;
    UWideString theTypeName;
    UWideString theTypeText;
    UWideString thePlayerName;
    UWideString theDetail;
    theText = inEffect->mName;
    if (theText.GetLength() > 0 && theText[0] == '<') {
        unsigned long thePlayer = inControls->mPlayers[0]->GetSelectedPlayer();
        if (thePlayer <= 6)
            gResource->GetString(thePlayer + IDS_TRIGGER_PLAYER_FIRST, thePlayerName);
        else {
            gResource->GetString(IDS_TRIGGER_PLAYER, theFormat);
            sprintf(thePlayerName, theFormat, thePlayer - 6);
        }
        gResource->GetString(this->mCurrentType + IDS_TRIGGER_PLAYER_EFFECT_FIRST, theTypeText);
        unsigned long thePosition = theTypeText.Find(" ", 0);
        if (thePosition > 0)
            theTypeName.CopyString(theTypeText, thePosition);
        else
            theTypeName = theTypeText;
        switch (this->mCurrentType) {
        case kSMPlayerActionDiplomacy: {
            unsigned long theOther = inControls->mPlayers[1]->GetSelectedPlayer();
            if (theOther <= 6)
                gResource->GetString(theOther + IDS_TRIGGER_PLAYER_FIRST, theDetail);
            else {
                gResource->GetString(IDS_TRIGGER_PLAYER, theFormat);
                sprintf(theDetail, theFormat, theOther - 6);
            }
            theDetail += " ";
            theDetail += inControls->mEffectComboBoxes[1]->GetTextLabel()->GetText(0);
            break;
        }
        case kSMPlayerActionSetAttributeLevel:
            theDetail = inControls->mEffectComboBoxes[1]->GetTextLabel()->GetText(0);
            break;
        case kSMPlayerActionModifyTechTree:
            theDetail += inControls->mRadioButtons[2]->GetIsChecked() ? " + " : " - ";
            theDetail += inControls->mEffectComboBoxes[1]->GetTextLabel()->GetText(0);
            break;
        case kSMPlayerActionAwardCivPoints:
            theDetail += inControls->mEditBoxes[0]->GetText(0);
            break;
        }
        theText = "<" + theTypeName + " " + thePlayerName + " " + theDetail + ">";
        inEffect->mName = theText;
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->mNameEditBox->SetText(theText);
    }
}

// SYNTHETIC: EXE 0x58a1d3
// (EESETSETPlayerTechnology's scalar deleting destructor)

// FUNCTION: EXE 0x58a1fe
// Stores the number of civilization points typed in the edit box
void EESETSETPlayerCivPoint::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    inEffect->mCivPoints = inControls->mEditBoxes[0]->GetTextAsInt(0);
}

// FUNCTION: EXE 0x58a222
// Hides the 'amount' label and edit box of the award civ points action
void EESETSETPlayerCivPoint::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58a23f
// Selects the effect's player without firing the picker event and fills the civ points edit box
void EESETSETPlayerCivPoint::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
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
    inControls->mEditBoxes[0]->SetText(inEffect->mCivPoints);
}

// FUNCTION: EXE 0x58a2ac
// Shows the 'amount' label and edit box (reset to 1) and sets the player picker's flags for the civ points action
void EESETSETPlayerCivPoint::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_AMOUNT, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 75.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY((float)(inControls->mLabels[3]->GetRectangle().GetBottom() + 5.0));
    inControls->mEditBoxes[0]->SetText((unsigned long)1);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    inControls->mPlayers[0]->SetFlags(0x88);
}

// FUNCTION: EXE 0x58a38d
// Stores the other player and the diplomacy stance chosen in the diplomacy action
void EESETSETPlayerDiplomacy::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inEffect->mDiplomacyPlayer = inControls->mPlayers[1]->GetSelectedPlayer();
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    inEffect->mDiplomacy = theSelection;
}

// FUNCTION: EXE 0x58a3b7
// Hides the 'to' and player labels, the other-player picker and the diplomacy label and combo
void EESETSETPlayerDiplomacy::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mPlayers[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58a3f1
// Selects the effect's player, other player and stance without firing their selection events
void EESETSETPlayerDiplomacy::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
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
    theAction = inControls->mPlayers[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mPlayers[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[1]->SetSelectedPlayer(inEffect->mDiplomacyPlayer);
    if (theAction) {
        inControls->mPlayers[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    theAction = inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mDiplomacy);
    if (theAction) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x58a4cc
// Shows the 'to' label, the other-player picker and the diplomacy combo (allied ... enemy) of the diplomacy action
void EESETSETPlayerDiplomacy::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    gResource->GetString(IDS_TO, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(666.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 20.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetAlignment(kTextAlignCenter);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetX(inControls->mLabels[3]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(666.0f);
    inControls->mPlayers[1]->SetLocation(theLocation);
    inControls->mPlayers[1]->SetFlags(0x99);
    inControls->mPlayers[1]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TRIGGER_EDITOR_DIPLOMACY, theText);
    theLocation.SetY(inControls->mPlayers[1]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 100.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[5]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->ChangeWidth(100);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (unsigned long theIndex = IDS_ALLIED; theIndex <= IDS_TRIGGER_ENEMY; theIndex++) {
        gResource->GetString(theIndex, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    inControls->mPlayers[0]->SetFlags(0x99);
}

// FUNCTION: EXE 0x58a702
// Stores the chosen player attribute, the change mode (exactly / increase / decrease) and the amount
void EESETSETPlayerResource::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    inEffect->mPlayerAttribute = theSelection;
    inControls->mComboBoxes[2]->GetCurrentSelection(theSelection);
    inEffect->mPlayerAttributeMode = theSelection;
    inEffect->mPlayerAttributeAmount = inControls->mEditBoxes[0]->GetTextAsInt(0);
}

// FUNCTION: EXE 0x58a746
// Hides the attribute label and combo, the mode combo and the amount label and edit box
void EESETSETPlayerResource::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58a780
// Selects the effect's player, attribute and mode (only if they differ; attribute event held off) and fills the amount
void EESETSETPlayerResource::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    UIEvent* theAction = inControls->mPlayers[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mPlayer);
    if (theAction) {
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    theAction = inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    if (inEffect->mPlayerAttribute != theSelection)
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(inEffect->mPlayerAttribute);
    if (theAction) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    inControls->mComboBoxes[2]->GetCurrentSelection(theSelection);
    if (inEffect->mPlayerAttributeMode != theSelection)
        inControls->mComboBoxes[2]->SetCurrentSelection(inEffect->mPlayerAttributeMode);
    inControls->mEditBoxes[0]->SetText(inEffect->mPlayerAttributeAmount);
}

// FUNCTION: EXE 0x58a85b
// Shows the attribute combo (resources, variables, sent/received, statistics, settings), mode combo and amount box
// (two statistics are listed as 'for future use')
void EESETSETPlayerResource::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    UWideString theLine;
    UWideString theSecond;
    UWideString theFirst;
    GE2DPoint theLocation;
    unsigned long i;
    unsigned long j;
    gResource->GetString(IDS_TRIGGER_EDITOR_ATTRIBUTE, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(641.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 75.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[3]->GetRectangle().GetRight() + 5.0f);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
    for (i = 0; i <= 10; i++) {
        gResource->GetString(i + IDS_TRIGGER_RESOURCE_FIRST, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    gResource->GetString(IDS_VARIABLE, theText);
    for (j = i; j < i + 10; j++) {
        sprintf(theLine, "%s%d", (const char*)theText, j - i);
        inControls->mEffectComboBoxes[1]->AppendLine(theLine);
    }
    for (j = IDS_RESOURCE_NAME_FIRST; j <= IDS_RESOURCE_NAME_IRON; j++) {
        gResource->GetString(j, theFirst);
        gResource->GetString(IDS_SENT, theSecond);
        sprintf(theLine, "%s %s", (const char*)theFirst, (const char*)theSecond);
        inControls->mEffectComboBoxes[1]->AppendLine(theLine);
        gResource->GetString(IDS_RECEIVED, theSecond);
        sprintf(theLine, "%s %s", (const char*)theFirst, (const char*)theSecond);
        inControls->mEffectComboBoxes[1]->AppendLine(theLine);
    }
    for (j = IDS_BUILDINGS_LOST; j <= IDS_STAT_FOR_FUTURE_USE6; j++) {
        if (j - 0x79e0 == 22 || j - 0x79e0 == 23)   // the statistic in this loop (j), not the finished counter i
            gResource->GetString(IDS_STAT_FOR_FUTURE_USE6, theText);
        else
            gResource->GetString(j, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    for (i = 0; i <= 5; i++) {
        gResource->GetString(i + IDS_TRIGGER_PLAYER_SETTING_FIRST, theText);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->GetGEText()->SetFont(gU531930);
    inControls->mEffectComboBoxes[1]->ChangeWidth(0x7d);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mComboBoxes[2]->DeleteAllEntries();
    for (i = IDS_TO_EXACTLY; i <= IDS_TO_DECREASE_BY; i++) {
        gResource->GetString(i, theText);
        inControls->mComboBoxes[2]->AppendLine(theText);
    }
    inControls->mComboBoxes[2]->ChangeWidth(0xa5);
    inControls->mComboBoxes[2]->SetLocation(theLocation);
    inControls->mComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_AMOUNT, theText);
    theLocation.SetY(inControls->mComboBoxes[2]->GetRectangle().GetBottom() + 5.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 75.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(theText);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mComboBoxes[2]->GetRectangle().GetRight() - inControls->mEditBoxes[0]->GetWidth());
    inControls->mEditBoxes[0]->SetText((unsigned long)0);
    inControls->mEditBoxes[0]->SetLocation(theLocation);
    inControls->mEditBoxes[0]->SetDisplayMode(kDisplayModeShown);
    inControls->mPlayers[0]->SetFlags(0x98);
}

// FUNCTION: EXE 0x58ac29
// Stores the technology chosen in the combo and whether it is added to or removed from the tech tree
void EESETSETPlayerTechnology::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inControls->mEffectComboBoxes[1]->GetCurrentSelection(theSelection);
    inEffect->mTechnology = this->mTechnologies[theSelection];
    inEffect->mIsAddTechnology = inControls->mRadioButtons[2]->GetIsChecked();
}

// FUNCTION: EXE 0x58ac5b
// Hides the add/remove radios, the technology label and the technology combo
void EESETSETPlayerTechnology::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mRadioButtons[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58ac92
// Selects the effect's player and technology (else the first) and checks add or remove
void EESETSETPlayerTechnology::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
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
    // the original drops this result and keeps testing the first action (a source bug, 0x18acf1)
    inControls->mEffectComboBoxes[1]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    long theValue = inEffect->mTechnology;
    std::vector<long>::iterator theIterator = std::find(this->mTechnologies.begin(), this->mTechnologies.end(), theValue);
    if (theIterator != this->mTechnologies.end())
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(theIterator - this->mTechnologies.begin());
    else
        inControls->mEffectComboBoxes[1]->SetCurrentSelection(0);
    if (theAction) {
        inControls->mEffectComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    if (inEffect->mIsAddTechnology)
        inControls->mRadioButtons[2]->Check();
    else
        inControls->mRadioButtons[3]->Check();
}

// the technology combo box sorts its entries by name (as EESETSConditionType.cpp's EESETSCTResearchNameLess, whose
// std::sort 0x1805c0 the original calls here: the functor is probably shared through a header; ours is a unit copy)
// (provisional name)
struct EESETSETTechnologyNameLess {
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

// NEAR: the original sorts with EESETSConditionType.cpp's EESETSCTResearchNameLess sort (0x1805c0; the functor is shared in the original, ours is a unit copy) and differs in the erase loop
// FUNCTION: EXE 0x58adb4
// Shows the add/remove radios and the technology combo; builds the sorted editor tech list on the first call
void EESETSETPlayerTechnology::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    UWideString theText;
    GE2DPoint theLocation;
    GEText* theGEText;
    // Built on the first Show of this type object, which is recreated with every scenario editor (a run-wide
    // flag left a later editor session with an empty list).
    if (this->mTechnologies.empty()) {
        gTechTree.GetNodeIDs(this->mTechnologies);
        for (unsigned long theEpoch = 0; theEpoch < 15; theEpoch++)
            this->mTechnologies.push_back((long)theEpoch);
        unsigned long theIndex = 0;
        while (theIndex < this->mTechnologies.size()) {
            if (gTechTree.Get(this->mTechnologies[theIndex])->mData->mShowInEditor == true)
                theIndex++;
            else
                this->mTechnologies.erase(this->mTechnologies.begin() + theIndex);
        }
        std::sort(this->mTechnologies.begin(), this->mTechnologies.end(), EESETSETTechnologyNameLess());
    }
    gResource->GetString(IDS_TRIGGER_EDITOR_ADD, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(616.0f);
    inControls->mRadioButtons[2]->SetText(theText, gU531934);
    inControls->mRadioButtons[2]->GetTextLabel()->GetGEText()->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inControls->mRadioButtons[2]->GetTextLabel()->GetGEText()->SetIsTransparent(true);
    inControls->mRadioButtons[2]->SetLocation(theLocation);
    inControls->mRadioButtons[2]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_SE_REMOVE, theText);
    theLocation.SetY(inControls->mRadioButtons[2]->GetRectangle().GetBottom() + 1.0f);
    inControls->mRadioButtons[3]->SetText(theText, gU531934);
    theGEText = inControls->mRadioButtons[3]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inControls->mRadioButtons[3]->SetLocation(theLocation);
    inControls->mRadioButtons[3]->SetDisplayMode(kDisplayModeShown);
    gResource->GetString(IDS_TECHNOLOGY, theText);
    theLocation.SetX(405.0f);
    theLocation.SetY(666.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 254.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[3]->SetRectangle(theRect);
    inControls->mLabels[3]->SetText(theText);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[3]->GetRectangle().GetBottom() + 3.0f);
    inControls->mEffectComboBoxes[1]->DeleteAllEntries();
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
        if (theNode && theText == "FONTS")
            sprintf(theText, "BAD Node %d - (name not available in release)", theNode->mData->mID);
        inControls->mEffectComboBoxes[1]->AppendLine(theText);
    }
    inControls->mEffectComboBoxes[1]->ChangeWidth(0xfe);
    inControls->mEffectComboBoxes[1]->SetLocation(theLocation);
    inControls->mEffectComboBoxes[1]->SetDisplayMode(kDisplayModeShown);
    inControls->mPlayers[0]->SetFlags(0x98);
}

// FUNCTION: EXE 0x58b156
// Selects the effect's player without firing the picker event (victory and defeat actions)
void EESETSETPlayerVictory::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    UIEvent* theAction = inControls->mPlayers[0]->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction) {
        theAction->Reference();
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    }
    inControls->mPlayers[0]->SetSelectedPlayer(inEffect->mPlayer);
    if (theAction) {
        inControls->mPlayers[0]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x58b1b0
// Switches a scenario combo to areas or objects: sets its label, fills it with the names and selects the first silently
void FillAreaObjectComboBox(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, bool inAreas)
{
    std::vector<UWideString> theNames;
    UWideString theText;
    if (inAreas) {
        inComboBox->mMode = kSETriggerScreenArea;
        gResource->GetString(IDS_AREA, theText);
        gWorld.mScenarioManager->GetAreaNames(theNames);
    } else {
        inComboBox->mMode = kSETriggerScreenObject;
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
        gWorld.mScenarioManager->GetObjectNames(theNames);
    }
    inLabel->SetText(theText);
    inComboBox->DeleteAllEntries();
    for (unsigned long i = 0; i < theNames.size(); i++)
        inComboBox->AppendLine(theNames[i]);
    UIEvent* theAction = inComboBox->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inComboBox->SetCurrentSelection(0);
    if (theAction) {
        inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x58b2dc
// Refills the location combo (0x99e) with areas or objects when the area (0x988) or object radio is checked
long EESEEUseAreaObject::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControlById(0x96d));
    EESETSEScenarioComboBox* theComboBox = static_cast<EESETSEScenarioComboBox*>(inForm->GetControlById(0x99e));
    FillAreaObjectComboBox(theLabel, theComboBox, static_cast<UIControl*>(inEntity)->GetId() == 0x988);
    return kErrorOK;
}

// EESEEMoveAreaObject::ProcessEvent: refills the camera move-to combo the same way; ICF-folded into
// EESEEUseAreaObject's 0x58b2dc (no address of its own)
long EESEEMoveAreaObject::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControlById(0x96d));
    EESETSEScenarioComboBox* theComboBox = static_cast<EESETSEScenarioComboBox*>(inForm->GetControlById(0x99e));
    FillAreaObjectComboBox(theLabel, theComboBox, static_cast<UIControl*>(inEntity)->GetId() == 0x988);
    return kErrorOK;
}

// FUNCTION: EXE 0x58b31f
// Refills the camera face-to combo (0x99f) with areas or objects when its area (0x98a) or object radio is checked
long EESEEFaceAreaObject::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControlById(0x96e));
    EESETSEScenarioComboBox* theComboBox = static_cast<EESETSEScenarioComboBox*>(inForm->GetControlById(0x99f));
    FillAreaObjectComboBox(theLabel, theComboBox, static_cast<UIControl*>(inEntity)->GetId() == 0x98a);
    return kErrorOK;
}

// FUNCTION: EXE 0x58b362
// Stores the camera's move-to and face-to area or object, follow/track flags, scroll speed and zoom; drops radio events
void EESETSETPlayerView::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inEffect->mUsesLocationArea = inControls->mRadioButtons[0]->GetIsChecked();
    if (inControls->mScenarioComboBoxes[1]->GetCurrentSelection(theSelection) == kErrorOK) {
        if (inEffect->mUsesLocationArea)
            inEffect->mLocationArea = theSelection;
        else
            inEffect->mLocationObject = theSelection;
    }
    inEffect->mUsesTargetArea = inControls->mRadioButtons[2]->GetIsChecked();
    if (inControls->mScenarioComboBoxes[2]->GetCurrentSelection(theSelection) == kErrorOK) {
        if (inEffect->mUsesTargetArea)
            inEffect->mTargetArea = theSelection;
        else
            inEffect->mTargetObject = theSelection;
    }
    inEffect->mIsAttack = inControls->mCheckBoxes[1]->GetIsChecked();
    inEffect->mUsesFavorites = inControls->mCheckBoxes[0]->GetIsChecked();
    inEffect->mIntValue1 = inControls->mSliders[0]->GetCurrentRangeValue();
    inEffect->mIntValue2 = inControls->mSliders[1]->GetCurrentRangeValue();
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[3]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[2]->SetTriggerEvent(kControlTriggerChecked, NULL);
}

// FUNCTION: EXE 0x58b456
// Hides the camera script controls, detaches their events and moves the player label and picker back to the top row
void EESETSETPlayerView::VirtualC(EESETSEffectControls* inControls)
{
    GE2DPoint theLocation;
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[6]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mRadioButtons[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mSliders[1]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mLabels[7]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[9]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[8]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[10]->SetDisplayMode(kDisplayModeHidden);
    inControls->mScenarioComboBoxes[1]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeShown);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[3]->SetTriggerEvent(kControlTriggerChecked, NULL);
    inControls->mRadioButtons[2]->SetTriggerEvent(kControlTriggerChecked, NULL);
    theLocation.SetX(220.0f);
    theLocation.SetY(641.0f);
    inControls->mLabels[2]->SetLocation(theLocation);
    theLocation.SetY(666.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
}

// NEAR: register allocation of the trigger-event juggling (as EESETSETMiniMapFlare::Load)
// FUNCTION: EXE 0x58b5cc
// Fills the camera script controls from the effect (move/face targets, flags, sliders) and attaches the radio events
void EESETSETPlayerView::Load(EESMEffect* inEffect, EESETSEffectControls* inControls)
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
    if (inEffect->mUsesTargetArea) {
        inControls->mRadioButtons[2]->Check();
        FillAreaObjectComboBox(inControls->mLabels[4], inControls->mScenarioComboBoxes[2], true);
        theAction = inControls->mScenarioComboBoxes[2]->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theAction)
            theAction->Reference();
        inControls->mScenarioComboBoxes[2]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        inControls->mScenarioComboBoxes[2]->SetCurrentSelection(inEffect->mTargetArea);
    } else {
        inControls->mRadioButtons[3]->Check();
        FillAreaObjectComboBox(inControls->mLabels[4], inControls->mScenarioComboBoxes[2], false);
        theAction = inControls->mScenarioComboBoxes[2]->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theAction)
            theAction->Reference();
        inControls->mScenarioComboBoxes[2]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        inControls->mScenarioComboBoxes[2]->SetCurrentSelection(inEffect->mTargetObject);
    }
    if (theAction) {
        inControls->mScenarioComboBoxes[2]->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
    if (inControls->mCheckBoxes[1]->GetIsChecked() != inEffect->mIsAttack)
        inControls->mCheckBoxes[1]->SetChecked(inEffect->mIsAttack);
    if (inControls->mCheckBoxes[0]->GetIsChecked() != inEffect->mUsesFavorites)
        inControls->mCheckBoxes[0]->SetChecked(inEffect->mUsesFavorites);
    if (inControls->mSliders[0]->GetCurrentRangeValue() != inEffect->mIntValue1)
        inControls->mSliders[0]->SetCurrentRangeValue(inEffect->mIntValue1);
    inControls->mSliders[0]->FireTriggerEvent(kControlTriggerRangeChanged);
    if (inControls->mSliders[1]->GetCurrentRangeValue() != inEffect->mIntValue2)
        inControls->mSliders[1]->SetCurrentRangeValue(inEffect->mIntValue2);
    inControls->mSliders[1]->FireTriggerEvent(kControlTriggerRangeChanged);
    UIEvent* theEvent = new EESEEMoveAreaObject;
    inControls->mRadioButtons[0]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[1]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
    theEvent = new EESEEFaceAreaObject;
    inControls->mRadioButtons[2]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    inControls->mRadioButtons[3]->SetTriggerEvent(kControlTriggerChecked, theEvent);
    theEvent->Dereference();
}

// SYNTHETIC: EXE 0x58b8a1
// (EESEEMoveAreaObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x58b8cc
// (EESEEFaceAreaObject's scalar deleting destructor)

// FUNCTION: EXE 0x58b8f7
// Lays out the camera script controls: move/face radios and combos, follow/track boxes, zoom and scroll speed sliders
void EESETSETPlayerView::Show(EESETSEffectControls* inControls)
{
    GERectangle theRect;
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint theLocation(220.0f, 641.0f);
    GEText* theGEText;
    UIESliderChange* theEvent;
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
    theLocation.SetX(220.0f);
    theLocation.SetY(inControls->mRadioButtons[1]->GetRectangle().GetBottom() + 3.0f);
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
    theLocation.SetX(10.0f);
    theLocation.SetY(690.0f);
    inControls->mLabels[2]->SetLocation(theLocation);
    theLocation.SetY(713.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0x88);
    theLocation.SetX(220.0f);
    theLocation.SetY(616.0f);
    gResource->GetString(IDS_MOVE_CAMERA, theText);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[5]->SetRectangle(theRect);
    inControls->mLabels[5]->SetText(theText);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[5]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(641.0f);
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
    theLocation.SetX(inControls->mLabels[5]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mRadioButtons[3]->GetRectangle().GetBottom() + 3.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 18.0f));
    inControls->mLabels[4]->SetRectangle(theRect);
    inControls->mLabels[4]->SetText(UWideString("----"));
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetY(inControls->mLabels[4]->GetRectangle().GetBottom() + 1.0f);
    inControls->mScenarioComboBoxes[2]->SetLocation(theLocation);
    inControls->mScenarioComboBoxes[2]->ChangeWidth(0xaf);
    inControls->mScenarioComboBoxes[2]->DeleteAllEntries();
    inControls->mScenarioComboBoxes[2]->SetDisplayMode(kDisplayModeShown);
    inControls->mPlayers[0]->SetFlags(0x88);
    theLocation.SetX(10.0f);
    theLocation.SetY(690.0f);
    inControls->mLabels[2]->SetLocation(theLocation);
    theLocation.SetY(713.0f);
    inControls->mPlayers[0]->SetLocation(theLocation);
    inControls->mPlayers[0]->SetFlags(0x88);
    theLocation.SetX(inControls->mLabels[5]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(616.0f);
    gResource->GetString(IDS_FACE_CAMERA, theText);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 175.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[6]->SetRectangle(theRect);
    inControls->mLabels[6]->SetText(theText);
    inControls->mLabels[6]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[6]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(616.0f);
    gResource->GetString(IDS_FOLLOW, theText);
    inControls->mCheckBoxes[1]->SetLocation(theLocation);
    inControls->mCheckBoxes[1]->SetText(theText, gU531934);
    inControls->mCheckBoxes[1]->ChangeWidth(100);
    inControls->mCheckBoxes[1]->SetDisplayMode(kDisplayModeShown);
    theGEText = inControls->mCheckBoxes[1]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    theLocation.SetX(inControls->mCheckBoxes[1]->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(616.0f);
    gResource->GetString(IDS_TRACK, theText);
    inControls->mCheckBoxes[0]->SetLocation(theLocation);
    inControls->mCheckBoxes[0]->SetText(theText, gU531934);
    inControls->mCheckBoxes[0]->ChangeWidth(100);
    inControls->mCheckBoxes[0]->SetDisplayMode(kDisplayModeShown);
    theGEText = inControls->mCheckBoxes[0]->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    theLocation.SetX(inControls->mLabels[6]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mRadioButtons[3]->GetRectangle().GetBottom() + 3.0f);
    gResource->GetString(IDS_ZOOM_LEVEL, theText);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 140.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[8]->SetRectangle(theRect);
    inControls->mLabels[8]->SetText(theText);
    inControls->mLabels[8]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[8]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 70.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[10]->SetRectangle(theRect);
    inControls->mLabels[10]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[6]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mLabels[8]->GetRectangle().GetBottom() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 210.0f, theLocation.GetY() + 20.0f));
    inControls->mSliders[1]->SetRectangle(theRect);
    inControls->mSliders[1]->SetRange(0, 0x65);
    inControls->mSliders[1]->SetPageSize(1);
    inControls->mSliders[1]->SetDisplayMode(kDisplayModeShown);
    theEvent = new UIESliderChange(inControls->mLabels[10], 1.0f, -51);
    gResource->GetString(IDS_AIPARAM_NONE, theText);
    theEvent->AddString(-51.0f, UWideString("Current"));
    theEvent->AddString(-50.0f, UWideString("HighPitch"));
    theEvent->AddString(-1.0f, UWideString("LowPitch"));
    theEvent->AddString(0.0f, UWideString("None"));
    theEvent->AddString(1.0f, UWideString("Close In"));
    theEvent->AddString(50.0f, UWideString("UpFarBack"));
    inControls->mSliders[1]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    inControls->mLabels[10]->SetText(inControls->mSliders[1]->GetCurrentRangeValue() - 10);
    theLocation.SetX(inControls->mLabels[6]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(641.0f);
    gResource->GetString(IDS_SCROLL_SPEED, theText);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 140.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[7]->SetRectangle(theRect);
    inControls->mLabels[7]->SetText(theText);
    inControls->mLabels[7]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[7]->GetRectangle().GetRight() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 70.0f, theLocation.GetY() + 20.0f));
    inControls->mLabels[9]->SetRectangle(theRect);
    inControls->mLabels[9]->SetDisplayMode(kDisplayModeShown);
    theLocation.SetX(inControls->mLabels[6]->GetRectangle().GetRight() + 5.0f);
    theLocation.SetY(inControls->mLabels[7]->GetRectangle().GetBottom() + 1.0f);
    theRect = GERectangle(theLocation, GE2DPoint(theLocation.GetX() + 210.0f, theLocation.GetY() + 20.0f));
    inControls->mSliders[0]->SetRectangle(theRect);
    inControls->mSliders[0]->SetRange(0, 100);
    inControls->mSliders[0]->SetPageSize(1);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeShown);
    theEvent = new UIESliderChange(inControls->mLabels[9], 1.0f, 0);
    gResource->GetString(IDS_SNAP, theText);
    theEvent->AddString(0.0f, UWideString(theText));
    gResource->GetString(IDS_INGAME_FAST, theText);
    theEvent->AddString(1.0f, theText);
    gResource->GetString(IDS_INGAME_SLOW, theText);
    theEvent->AddString(100.0f, theText);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    inControls->mLabels[9]->SetText(inControls->mSliders[0]->GetCurrentRangeValue());
    inControls->mScenarioComboBoxes[1]->GetForm()->GetControlById(8)->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x58c4e3
// Shows the value ranges of the AI attribute just picked in the attribute combo
long EESETSETSAAAttributeChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theSelection;
    static_cast<UIComboBox*>(inEntity)->GetCurrentSelection(theSelection);
    this->mType->ShowAttributeRanges(theSelection, this->mControls);
    return kErrorOK;
}

// NEAR: the original calls _Ptrit::operator+ for the first mAttributes[theSelection] (inline budget) and caches this/the selection in frame slots
// FUNCTION: EXE 0x58c4fd
// Stores the chosen AI attribute, its two integer values from the value labels and the slider value / behavior scale
void EESETSETSetAIAttribute::Store(EESMEffect* inEffect, EESETSEffectControls* inControls)
{
    unsigned long theSelection;
    inControls->mComboBoxes[1]->GetCurrentSelection(theSelection);
    EECPBehavior theBehavior = gCPBehaviors[this->mAttributes[theSelection]];
    inEffect->mAIAttribute = this->mAttributes[theSelection];
    inEffect->mIntValue1 = inControls->mLabels[6]->GetTextAsInt(0);
    inEffect->mIntValue2 = inControls->mLabels[9]->GetTextAsInt(0);
    inEffect->mFloatValue = (float)inControls->mSliders[2]->GetCurrentRangeValue() / theBehavior.mFloatSliderScale;
}

// FUNCTION: EXE 0x58c593
// Hides the AI attribute controls (labels, combo, three sliders), resets their alignment and string ids, drops events
void EESETSETSetAIAttribute::VirtualC(EESETSEffectControls* inControls)
{
    inControls->mLabels[4]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[7]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[10]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[3]->SetDisplayMode(kDisplayModeHidden);
    inControls->mComboBoxes[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[6]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[9]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[12]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[6]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[9]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[12]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[5]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[8]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[11]->SetDisplayMode(kDisplayModeHidden);
    inControls->mLabels[5]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[8]->SetAlignment(kTextAlignLeft);
    inControls->mLabels[11]->SetAlignment(kTextAlignLeft);
    inControls->mSliders[0]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[1]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[2]->SetDisplayMode(kDisplayModeHidden);
    inControls->mSliders[0]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mSliders[1]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mSliders[2]->SetTriggerEvent(kControlTriggerRangeChanged, NULL);
    inControls->mLabels[4]->SetStringTableID(0);
    inControls->mLabels[6]->SetStringTableID(0);
    inControls->mSliders[0]->SetStringTableID(0);
    inControls->mLabels[5]->SetStringTableID(0);
    inControls->mLabels[7]->SetStringTableID(0);
    inControls->mLabels[9]->SetStringTableID(0);
    inControls->mSliders[1]->SetStringTableID(0);
    inControls->mLabels[8]->SetStringTableID(0);
    inControls->mLabels[10]->SetStringTableID(0);
    inControls->mLabels[12]->SetStringTableID(0);
    inControls->mSliders[2]->SetStringTableID(0);
    inControls->mLabels[11]->SetStringTableID(0);
    inControls->mComboBoxes[1]->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
}
