// C:\EEX\Empire Earth\UI Events\EEUIEDiplomacyEvents.cpp (file name inferred: no __FILE__; class prefix EEUIEDS).
// The diplomacy form's events: 0x1d4165..0x1d6775 (the CCCE unit ends at 0x1d4165, EEUIEEmpireEarthEditorsEvents.cpp
// starts at 0x1d6775). Open, its ??_G block, the tribute helpers (external functions in definition order: as file
// statics VC6 would emit each after its first user, after LClick), the EEMAddRemoveResources ctor COMDAT after its first
// user, the ProcessEvents in vtable order with the message ctor COMDATs after OKButton. The two check boxes'
// ProcessEvents are the ICF-shared `return 0` (0x27cf38). The float constants are literals: their __real COMDATs sit in
// this unit's .rdata (0x44630c..0x44632c, 0x4463bc) because no earlier unit uses them.
// Open (0x1d4165) was built without /Og: every local memory resident, constants not folded into inline arguments,
// `this` saved to the frame, the first seven GEColor::operator= and all UIEvent() sites refused. It takes the rule-4
// `#pragma optimize("g", off)`; the frame layout then follows the local names' hash, so its locals' names are chosen
// by that hash (LEARNINGS round 14) and declared in the order the frame needs. Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <stdlib.h>
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
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"
#include "lle/UIBitmap.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/RTSWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIEDiplomacyEvents.h"
#include "ee/EEStringIDs.h"
long GetTributeLimit(UIForm* inForm, unsigned long inControl);
long GetTributeResource(unsigned long inControl);
void RefundTributes(UIForm* inForm);
void AddTribute(UIForm* inForm, long inResource, long inAmount);

// Built without /Og (the file header): the pragma plus this source shape make 0x5d4165 exact
#pragma optimize("g", off)
// (an inline forward: the original stores the player in a temporary before the local)
inline EEPlayer* GetDiplomacyPlayer(unsigned long inIndex) { return gWorld.GetPlayer(inIndex); }
// FUNCTION: EXE 0x5d4165
// Fills the diplomacy form: the player's status icon, name, civilization and resources, then a row per other
// player: icon, chat and allied boxes, stance, five tribute buttons (hidden for gone, observer, defeated players)
long EEUIEDSOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theNoTribute("0");
    UWideString theBlankText("");
    UWideString theFriend;
    UWideString theEnemy;
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRightTC;
    GETextureCoordinate theBottomRightPos;
    GETextureCoordinate theBottomLeftCell;
    EEPlayer* theLocalPlayer = GetDiplomacyPlayer(gWorld.mCurrentPlayer);
    unsigned long theLocalSlot = theLocalPlayer->mPlayerID;
    unsigned long theOpponentIndex = 1;
    gResource->GetString(IDS_FRIENDLY, theFriend);
    gResource->GetString(IDS_HOSTILE, theEnemy);
    UIEvent* theCustomEvent = new EEUIEDSCustomChatCheckBox;
    UIEvent* theAlliedCheckChanged = new EEUIEDSAlliedCheckBox;
    UIEvent* theLeftEvent = new EEUIEDSTributeLClick;
    UIEvent* theRClickEvent = new EEUIEDSTributeRClick;
    UIEvent* theOKEvent = new EEUIEDSOKButton;
    UIEvent* theClearButtonPressed = new EEUIEDSClearTribute;
    UIEvent* theCancelEvent = new EEUIEDSCancel;
    unsigned long theIndex = 11;
    UIBitmap* theBitmap = dynamic_cast<UIBitmap*>(inForm->GetControl(theIndex));
    if (theLocalPlayer->GetGameResult() == 2) {
        theTopLeft.mU = 0.5859375f;
        theTopLeft.mV = 0.0f;
        theBottomLeftCell.mU = 0.7734375f;
        theBottomLeftCell.mV = 0.1875f;
        theTopRightTC.mU = 0.7734375f;
        theTopRightTC.mV = 0.0f;
        theBottomRightPos.mU = 0.5859375f;
        theBottomRightPos.mV = 0.1875f;
    } else if (theLocalPlayer->GetObserverMode()) {
        theTopLeft.mU = 0.390625f;
        theTopLeft.mV = 0.0f;
        theBottomLeftCell.mU = 0.578125f;
        theBottomLeftCell.mV = 0.1875f;
        theTopRightTC.mU = 0.578125f;
        theTopRightTC.mV = 0.0f;
        theBottomRightPos.mU = 0.390625f;
        theBottomRightPos.mV = 0.1875f;
    } else {
        theTopLeft.mU = 0.0f;
        theTopLeft.mV = 0.0f;
        theBottomLeftCell.mU = 0.1875f;
        theBottomLeftCell.mV = 0.1875f;
        theTopRightTC.mU = 0.1875f;
        theTopRightTC.mV = 0.0f;
        theBottomRightPos.mU = 0.0f;
        theBottomRightPos.mV = 0.1875f;
    }
    theBitmap->GetBitmap()->SetTextureCoordinates(theTopLeft, theTopRightTC, theBottomRightPos, theBottomLeftCell);
    theBitmap->SetBgColor(GEColor(0.667f, 0.616f, 0.0f));
    theIndex++;
    UITextLabel* theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(theLocalPlayer->mName);
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(UWideString(theLocalPlayer->mCivilization.mName));
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(theLocalPlayer->GetResource(kResourceFood));
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(theLocalPlayer->GetResource(kResourceWood));
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(theLocalPlayer->GetResource(kResourceStone));
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(theLocalPlayer->GetResource(kResourceGold));
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    theTextLabel->SetText(theLocalPlayer->GetResource(kResourceIron));
    theTextLabel->GetGEText()->SetFgColor(theLocalPlayer->GetColor());
    theIndex++;
    EEPlayer* theOtherPlayer;
    bool theHideRow;
    UICheckBox* theCheckBox;
    UIButton* theButton = dynamic_cast<UIButton*>(inForm->GetControl(kDiplomacyOKButton));
    theButton->SetTrigger(kControlTriggerButtonUp, theOKEvent);
    theButton = dynamic_cast<UIButton*>(inForm->GetControl(kDiplomacyClearTributesButton));
    theButton->SetTrigger(kControlTriggerButtonUp, theClearButtonPressed);
    theButton = dynamic_cast<UIButton*>(inForm->GetControl(kDiplomacyCancelButton));
    theButton->SetTrigger(kControlTriggerButtonUp, theCancelEvent);
    theIndex = 19;
    if (theOpponentIndex == theLocalSlot)
        theOpponentIndex++;
    while (theIndex <= 183) {
        if (theOpponentIndex < gWorld.GetNumPlayers())
            theOtherPlayer = GetDiplomacyPlayer(theOpponentIndex);
        else
            theOtherPlayer = NULL;
        theHideRow = theOtherPlayer == NULL || theOtherPlayer->GetObserverMode() ||
                         (theOtherPlayer->IsHuman() && theOtherPlayer->GetSlot() == kSlotNone) || theOtherPlayer->GetGameResult() == 2;
        theBitmap = dynamic_cast<UIBitmap*>(inForm->GetControl(theIndex));
        if (theOtherPlayer == NULL)
            theBitmap->SetHiddenMode();
        else {
            if (theOtherPlayer->GetGameResult() == 2) {
                theTopLeft.mU = 0.5859375f;
                theTopLeft.mV = 0.0f;
                theBottomLeftCell.mU = 0.7734375f;
                theBottomLeftCell.mV = 0.1875f;
                theTopRightTC.mU = 0.7734375f;
                theTopRightTC.mV = 0.0f;
                theBottomRightPos.mU = 0.5859375f;
                theBottomRightPos.mV = 0.1875f;
                if (theOtherPlayer->IsHuman())
                    theBitmap->SetBgColor(GEColor(0.667f, 0.616f, 0.0f));
                else
                    theBitmap->SetBgColor(GEColor(0.85f, 0.85f, 0.85f));
            } else if (theOtherPlayer->IsHuman()) {
                if (theOtherPlayer->GetSlot() == kSlotNone) {
                    theTopLeft.mU = 0.1953125f;
                    theTopLeft.mV = 0.0f;
                    theBottomLeftCell.mU = 0.3828125f;
                    theBottomLeftCell.mV = 0.1875f;
                    theTopRightTC.mU = 0.3828125f;
                    theTopRightTC.mV = 0.0f;
                    theBottomRightPos.mU = 0.1953125f;
                    theBottomRightPos.mV = 0.1875f;
                } else if (theOtherPlayer->GetObserverMode()) {
                    theTopLeft.mU = 0.390625f;
                    theTopLeft.mV = 0.0f;
                    theBottomLeftCell.mU = 0.578125f;
                    theBottomLeftCell.mV = 0.1875f;
                    theTopRightTC.mU = 0.578125f;
                    theTopRightTC.mV = 0.0f;
                    theBottomRightPos.mU = 0.390625f;
                    theBottomRightPos.mV = 0.1875f;
                } else {
                    theTopLeft.mU = 0.0f;
                    theTopLeft.mV = 0.0f;
                    theBottomLeftCell.mU = 0.1875f;
                    theBottomLeftCell.mV = 0.1875f;
                    theTopRightTC.mU = 0.1875f;
                    theTopRightTC.mV = 0.0f;
                    theBottomRightPos.mU = 0.0f;
                    theBottomRightPos.mV = 0.1875f;
                }
                theBitmap->SetBgColor(GEColor(0.667f, 0.616f, 0.0f));
            } else {
                theTopLeft.mU = 0.0f;
                theTopLeft.mV = 0.0f;
                theBottomLeftCell.mU = 0.1875f;
                theBottomLeftCell.mV = 0.1875f;
                theTopRightTC.mU = 0.1875f;
                theTopRightTC.mV = 0.0f;
                theBottomRightPos.mU = 0.0f;
                theBottomRightPos.mV = 0.1875f;
                theBitmap->SetBgColor(GEColor(0.85f, 0.85f, 0.85f));
            }
            theBitmap->GetBitmap()->SetTextureCoordinates(theTopLeft, theTopRightTC, theBottomRightPos, theBottomLeftCell);
        }
        theIndex++;
        theCheckBox = dynamic_cast<UICheckBox*>(inForm->GetControl(theIndex));
        theCheckBox->SetChecked(theOtherPlayer == NULL ? false : theLocalPlayer->GetSendChatMessage(theOpponentIndex) == true);
        if (theHideRow)
            theCheckBox->SetHiddenMode();
        theCheckBox->SetTrigger(kControlTriggerChecked, theCustomEvent);
        theCheckBox->SetTrigger(kControlTriggerUnchecked, theCustomEvent);
        theIndex++;
        theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
        theTextLabel->SetText(theOtherPlayer == NULL ? theBlankText : theOtherPlayer->mName);
        if (theHideRow)
            theTextLabel->SetHiddenMode();
        else
            theTextLabel->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theIndex++;
        theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
        theTextLabel->SetText(theOtherPlayer == NULL ? theBlankText : UWideString(theOtherPlayer->mCivilization.mName));
        if (theHideRow)
            theTextLabel->SetHiddenMode();
        else
            theTextLabel->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theIndex++;
        theTextLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
        theTextLabel->SetText(theOtherPlayer == NULL ? theBlankText : (theOtherPlayer->GetEnemy(theLocalPlayer->GetPlayerID()) == 0 ? theFriend : theEnemy));
        if (theHideRow)
            theTextLabel->SetHiddenMode();
        else
            theTextLabel->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theIndex++;
        theCheckBox = dynamic_cast<UICheckBox*>(inForm->GetControl(theIndex));
        theCheckBox->SetChecked(theOtherPlayer == NULL ? false : theLocalPlayer->GetEnemy(theOpponentIndex) == 0);
        if (theHideRow)
            theCheckBox->SetHiddenMode();
        if (!gWorld.GetTeamsLocked()) {
            theCheckBox->SetTrigger(kControlTriggerChecked, theAlliedCheckChanged);
            theCheckBox->SetTrigger(kControlTriggerUnchecked, theAlliedCheckChanged);
        } else
            theCheckBox->SetDisabled(true);
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theNoTribute, theButton->GetGEText()->GetFont());
        if (theHideRow)
            theButton->SetHiddenMode();
        else
            theButton->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theButton->SetTrigger(kControlTriggerButtonUp, theLeftEvent);
        theButton->SetTrigger(kControlTriggerRightButtonUp, theRClickEvent);
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theNoTribute, theButton->GetGEText()->GetFont());
        if (theHideRow)
            theButton->SetHiddenMode();
        else
            theButton->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theButton->SetTrigger(kControlTriggerButtonUp, theLeftEvent);
        theButton->SetTrigger(kControlTriggerRightButtonUp, theRClickEvent);
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theNoTribute, theButton->GetGEText()->GetFont());
        if (theHideRow)
            theButton->SetHiddenMode();
        else
            theButton->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theButton->SetTrigger(kControlTriggerButtonUp, theLeftEvent);
        theButton->SetTrigger(kControlTriggerRightButtonUp, theRClickEvent);
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theNoTribute, theButton->GetGEText()->GetFont());
        if (theHideRow)
            theButton->SetHiddenMode();
        else
            theButton->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theButton->SetTrigger(kControlTriggerButtonUp, theLeftEvent);
        theButton->SetTrigger(kControlTriggerRightButtonUp, theRClickEvent);
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theNoTribute, theButton->GetGEText()->GetFont());
        if (theHideRow)
            theButton->SetHiddenMode();
        else
            theButton->GetGEText()->SetFgColor(theOtherPlayer->GetColor());
        theButton->SetTrigger(kControlTriggerButtonUp, theLeftEvent);
        theButton->SetTrigger(kControlTriggerRightButtonUp, theRClickEvent);
        theIndex++;
        theOpponentIndex++;
        if (theOpponentIndex == theLocalSlot)
            theOpponentIndex++;
    }
    theCustomEvent->Dereference();
    theAlliedCheckChanged->Dereference();
    theLeftEvent->Dereference();
    theRClickEvent->Dereference();
    theOKEvent->Dereference();
    theClearButtonPressed->Dereference();
    theCancelEvent->Dereference();
    return kErrorOK;
}

#pragma optimize("g", on)

// FUNCTION: EXE 0x5d5a0e
// Destroys the allied check box event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIEDSAlliedCheckBox, then CustomChatCheckBox's, TributeLClick's, TributeRClick's, OKButton's,
// ClearTribute's and Cancel's)

// FUNCTION: EXE 0x5d5a39
// Destroys the custom-chat check box event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d5a64
// Destroys the tribute left-click event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d5a8f
// Destroys the tribute right-click event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d5aba
// Destroys the diplomacy OK event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d5ae5
// Destroys the clear-tributes event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d5b10
// Destroys the diplomacy Cancel event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d5b3b
// Returns how much more of a tribute button's resource the player can give once the tribute fee is added
long GetTributeLimit(UIForm* inForm, unsigned long inControl)
{
    long theAvailable;
    float theFee;
    {
        UWideString theUnused;
        UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl((inControl - 19) % 11 + 8));
        theAvailable = theLabel->GetTextAsInt(0);
        theFee = gWorld.GetCurrentPlayer()->mTributeFee;
    }
    return (long)((theAvailable + 0.0001f) / (theFee + 1.0001f));
}

// FUNCTION: EXE 0x5d5bc1
// Returns the resource of a tribute button (2 food, 3 wood, 4 stone, 5 gold, 6 iron)
long GetTributeResource(unsigned long inControl)
{
    long theResource = 0;
    switch ((inControl - 19) % 11 - 6) {
    case 0:
        theResource = 2;
        break;
    case 1:
        theResource = 3;
        break;
    case 2:
        theResource = 4;
        break;
    case 3:
        theResource = 5;
        break;
    case 4:
        theResource = 6;
        break;
    }
    return theResource;
}

// FUNCTION: EXE 0x5d5bfc
// Gives back the resources plus fee held in every tribute button of the form
void RefundTributes(UIForm* inForm)
{
    UWideString theText;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    unsigned long theLocal = thePlayer->mPlayerID;
    unsigned long theIndex;
    long theResource2 = 0;
    long theResource3 = 0;
    long theResource4;
    long theResource5 = 0;
    theResource4 = 0;
    long theResource6 = 0;
    if (!thePlayer)
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        if (i == theLocal)
            continue;
        theIndex = i * 11 + 19;
        if (i > theLocal)
            theIndex -= 22;
        else
            theIndex -= 11;
        theIndex += 6;
        theText = dynamic_cast<UIButton*>(inForm->GetControl(theIndex))->GetGEText()->GetText();
        theResource2 += atoi(theText);
        theIndex++;
        theText = dynamic_cast<UIButton*>(inForm->GetControl(theIndex))->GetGEText()->GetText();
        theResource3 += atoi(theText);
        theIndex++;
        theText = dynamic_cast<UIButton*>(inForm->GetControl(theIndex))->GetGEText()->GetText();
        theResource4 += atoi(theText);
        theIndex++;
        theText = dynamic_cast<UIButton*>(inForm->GetControl(theIndex))->GetGEText()->GetText();
        theResource5 += atoi(theText);
        theIndex++;
        theText = dynamic_cast<UIButton*>(inForm->GetControl(theIndex))->GetGEText()->GetText();
        theResource6 += atoi(theText);
    }
    if (theResource2 > 0)
        AddTribute(inForm, 2, (long)(gWorld.GetCurrentPlayer()->GetTributeFee() * theResource2) + theResource2);
    if (theResource3 > 0)
        AddTribute(inForm, 3, (long)(gWorld.GetCurrentPlayer()->GetTributeFee() * theResource3) + theResource3);
    if (theResource4 > 0)
        AddTribute(inForm, 4, (long)(gWorld.GetCurrentPlayer()->GetTributeFee() * theResource4) + theResource4);
    if (theResource5 > 0)
        AddTribute(inForm, 5, (long)(gWorld.GetCurrentPlayer()->GetTributeFee() * theResource5) + theResource5);
    if (theResource6 > 0)
        AddTribute(inForm, 6, (long)(gWorld.GetCurrentPlayer()->GetTributeFee() * theResource6) + theResource6);
}

// FUNCTION: EXE 0x5d5eca
// Adds an amount (negative: removes) to the player's shown resource and posts that resource change for the player
void AddTribute(UIForm* inForm, long inResource, long inAmount)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    unsigned long theIndex = 13;
    if (!thePlayer)
        return;
    switch (inResource) {
    case 2:
        theIndex = 14;
        break;
    case 3:
        theIndex = 15;
        break;
    case 4:
        theIndex = 16;
        break;
    case 5:
        theIndex = 17;
        break;
    case 6:
        theIndex = 18;
        break;
    }
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(theIndex));
    long theValue = theLabel->GetTextAsInt(0);
    theLabel->SetText(theValue + inAmount);
    gCommunications->ProcessLocalUserInput(new EEMAddRemoveResources(thePlayer->GetPlayerID(), inResource,
                                                                     inAmount > 0 ? inAmount : -inAmount,
                                                                     inAmount > 0), kSlotNone);
}

// FUNCTION: EXE 0x5d5f8f
// Creates the message that adds or removes an amount of a player's resource
// (EEMAddRemoveResources's ctor, after its first user)

// FUNCTION: EXE 0x5d5fd1
// Raises a tribute by 100 (500 with Shift), capped at 99999 and what the player can afford; charges it plus the fee
long EEUIEDSTributeLClick::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UWideString theString;
    UIButton* theButton = dynamic_cast<UIButton*>(inEntity);
    ISEntity* theKey;
    gISKeyboard->GetEntity(kKeyShift, theKey);
    long theAmount = theKey->GetValue() > 0 ? 500 : 100;
    GEText* theGEText = theButton->GetGEText();
    theText = theGEText->GetText();
    long theOld = atoi(theText);
    long theLimit = GetTributeLimit(inForm, theButton->GetIndex()) + theOld;
    theAmount = min(theLimit, min(99999, theOld + theAmount));
    sprintf(theString, "%ld", theAmount);
    theButton->SetText(theString, theGEText->GetFont());
    if (theOld == theAmount)
        return kErrorOK;
    float theFee = gWorld.GetCurrentPlayer()->mTributeFee;
    theAmount = theOld - theAmount;
    AddTribute(inForm, GetTributeResource(theButton->GetIndex()), (long)((theAmount + 0.0001f) * (theFee + 1.0001f)));
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6113
// Lowers a tribute by 100 (500 with Shift) down to 0, giving back the difference plus the fee
long EEUIEDSTributeRClick::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UWideString theString;
    UIButton* theButton = dynamic_cast<UIButton*>(inEntity);
    ISEntity* theKey;
    gISKeyboard->GetEntity(kKeyShift, theKey);
    long theAmount = theKey->GetValue() > 0 ? 500 : 100;
    GEText* theGEText = theButton->GetGEText();
    theText = theGEText->GetText();
    long theOld = atoi(theText);
    theAmount = theOld - theAmount < 0 ? 0 : theOld - theAmount;
    sprintf(theString, "%ld", theAmount);
    theButton->SetText(theString, theGEText->GetFont());
    if (theOld != theAmount) {
        float theFee = gWorld.GetCurrentPlayer()->mTributeFee;
        theAmount = theOld - theAmount;
        AddTribute(inForm, GetTributeResource(theButton->GetIndex()), (long)((theAmount + 0.0001f) * (theFee + 1.0001f)));
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5d622f
// Posts every row's tribute, stance change (unless teams are locked) and custom-chat choice, then closes the form
long EEUIEDSOKButton::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    unsigned long theLocal = thePlayer->GetPlayerID();
    for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++) {
        if (i == theLocal)
            continue;
        unsigned long theIndex = i * 11 + 19;
        if (i > theLocal)
            theIndex -= 22;
        else
            theIndex -= 11;
        bool theCustomChat = dynamic_cast<UICheckBox*>(inForm->GetControl(++theIndex))->GetIsChecked();
        bool theAllied = dynamic_cast<UICheckBox*>(inForm->GetControl(theIndex += 4))->GetIsChecked();
        unsigned long theFood;
        unsigned long theWood;
        unsigned long theGold;
        unsigned long theStone;
        unsigned long theIron;
        theText = dynamic_cast<UIButton*>(inForm->GetControl(++theIndex))->GetGEText()->GetText();
        theFood = atoi(theText);
        theText = dynamic_cast<UIButton*>(inForm->GetControl(++theIndex))->GetGEText()->GetText();
        theWood = atoi(theText);
        theText = dynamic_cast<UIButton*>(inForm->GetControl(++theIndex))->GetGEText()->GetText();
        theStone = atoi(theText);
        theText = dynamic_cast<UIButton*>(inForm->GetControl(++theIndex))->GetGEText()->GetText();
        theGold = atoi(theText);
        theText = dynamic_cast<UIButton*>(inForm->GetControl(++theIndex))->GetGEText()->GetText();
        theIron = atoi(theText);
        if (theFood > 0 || theWood > 0 || theGold > 0 || theStone > 0 || theIron > 0)
            gCommunications->ProcessLocalUserInput(new EEMPayTribute((unsigned char)i, theFood, theWood, theGold,
                                                                     theStone, theIron), kSlotNone);
        if (!gWorld.GetTeamsLocked()) {
            if (theAllied != (thePlayer->mEnemies[i] == 0))
                gCommunications->ProcessLocalUserInput(new EEMSetDiplomaticStance(i, theAllied == false), kSlotNone);
        }
        if (theCustomChat != thePlayer->mSendChatMessage[i])
            gCommunications->ProcessLocalUserInput(new EEMSelectCustomPlayerForChat(i, theCustomChat), kSlotNone);
    }
    if (thePlayer)
        gUserInterface->CloseForm(NULL);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6542
// Creates the pay-tribute message (receiver and the food, wood, gold, stone and iron amounts)
// (EEMPayTribute's ctor, then EEMSelectCustomPlayerForChat's and EEMSetDiplomaticStance's, after their first user)

// FUNCTION: EXE 0x5d6590
// Creates the message that adds or removes a player from the custom chat recipients

// FUNCTION: EXE 0x5d65c4
// Creates the message that sets the diplomatic stance toward a player (ally or enemy)

// FUNCTION: EXE 0x5d65f8
// Gives back the pending tributes and resets every tribute button to 0
long EEUIEDSClearTribute::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theZero("0");
    UIButton* theButton;
    RefundTributes(inForm);
    for (long theIndex = 19; theIndex <= 183; theIndex++) {
        theIndex += 6;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theZero, theButton->GetGEText()->GetFont());
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theZero, theButton->GetGEText()->GetFont());
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theZero, theButton->GetGEText()->GetFont());
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theZero, theButton->GetGEText()->GetFont());
        theIndex++;
        theButton = dynamic_cast<UIButton*>(inForm->GetControl(theIndex));
        theButton->SetText(theZero, theButton->GetGEText()->GetFont());
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5d674a
// Gives back the pending tributes and closes the diplomacy form
long EEUIEDSCancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gWorld.GetCurrentPlayer()) {
        RefundTributes(inForm);
        gUserInterface->CloseForm(NULL);
    }
    return kErrorOK;
}

long EEUIEDSAlliedCheckBox::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return kErrorOK;
}

long EEUIEDSCustomChatCheckBox::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return kErrorOK;
}
