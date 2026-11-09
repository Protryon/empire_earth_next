// The moderator action dialog (warn, mute, unmute, kick, ban a player), 0x7f4a60..0x7f525d
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {

AdminActionDialog::AdminActionDialog() {}
// FUNCTION: WON 0x7f4ad0
// Destroys the moderator action dialog (members only)
AdminActionDialog::~AdminActionDialog() {}
// FUNCTION: WON 0x7f4c90
// Enables the action button when a reason is typed or the reason is optional for this action
void AdminActionDialog::UpdateDoActionButton()
{
    if (this->mExplanation->GetText().length() != 0 || this->mIsReasonOptional)
        this->mDoActionButton->SetEnabled(true);
    else
        this->mDoActionButton->SetEnabled(false);
}
// FUNCTION: WON 0x7f4ce0
// Rechecks the action button when the reason text changes; passes other events to the dialog
void AdminActionDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_InputTextChanged && inEvent->mComponent == this->mExplanation.get()) {
        this->UpdateDoActionButton();
        return;
    }
    Dialog::HandleComponentEvent(inEvent);
}
// FUNCTION: WON 0x007f4d10
// Returns whether the action applies to the whole server rather than the room
bool AdminActionDialog::GetApplyToServer()
{
    return this->mApplyToServer->IsChecked();
}
// FUNCTION: WON 0x007f4d20
// Returns the reason the moderator typed
const GUIString& AdminActionDialog::GetExplanation()
{
    return this->mExplanation->GetText();
}
// FUNCTION: WON 0x7f4d30
// Returns the chosen duration (1, 5, 10, 30 or 0 = forever) in seconds for minutes, hours or days
unsigned long AdminActionDialog::GetNumSeconds()
{
    unsigned long aSeconds = 3600;
    switch(this->mDurationCombo->GetSelItem()) {
        case 0: aSeconds=1; break;
        case 1: aSeconds=5; break;
        case 2: aSeconds=10; break;
        case 3: aSeconds=30; break;
        case 4: aSeconds=0; break;
    }
    switch(this->mTimeSegmentCombo->GetSelItem()) {
        case 2: aSeconds*=24;
        case 1: aSeconds*=60;
        case 0: aSeconds*=60;
    }
    return aSeconds;
}
// FUNCTION: WON 0x7f4dc0
// Finds the reason, duration, room/server scope, title and button controls in the layout
void AdminActionDialog::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType != 2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mExplanationHeader, "ExplanationHeader", __FILE__, 105);
    SafeGetComponent(aConfig, this->mExplanation, "Explanation", __FILE__, 106);
    SafeGetComponent(aConfig, this->mDurationHeader, "DurationHeader", __FILE__, 107);
    SafeGetComponent(aConfig, this->mDurationCombo, "DurationCombo", __FILE__, 108);
    SafeGetComponent(aConfig, this->mTimeSegmentCombo, "TimeSegmentCombo", __FILE__, 109);
    SafeGetComponent(aConfig, this->mDoActionButton, "DoActionButton", __FILE__, 110);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 111);
    SafeGetComponent(aConfig, this->mApplyToRoom, "ApplyToRoomRadio", __FILE__, 112);
    SafeGetComponent(aConfig, this->mApplyToServer, "ApplyToServerRadio", __FILE__, 113);
    SafeGetComponent(aConfig, this->mTitleLabel, "TitleLabel", __FILE__, 114);
}
// FUNCTION: WON 0x7f4ee0
// Sets the dialog up for one action on a player: title, button text and which reason/duration/scope show
void AdminActionDialog::Reset(int inAction, LobbyClient* inTarget)
{
    this->mActionType=inAction;
    GUIString anAction;
    bool showDuration,enableReason,showScope;
    this->mApplyToServer->SetCheck(false,false);
    this->mApplyToRoom->SetCheck(true,false);
    this->mExplanation->SetText(GUIString(""),false);
    this->mDurationCombo->SetSelItem(1,false);
    this->mTimeSegmentCombo->SetSelItem(0,false);
    switch(this->mActionType) {
        case 15: anAction=::AdminActionDialog_Warn_String; showDuration=false; enableReason=true; this->mIsReasonOptional=false; showScope=false; break;
        case 8: anAction=::AdminActionDialog_Mute_String; showDuration=true; enableReason=true; this->mIsReasonOptional=true; showScope=true; break;
        case 14: anAction=::AdminActionDialog_UnMute_String; showDuration=false; enableReason=false; this->mIsReasonOptional=true; showScope=true; break;
        case 7: anAction=::AdminActionDialog_Kick_String; showDuration=false; enableReason=true; this->mIsReasonOptional=true; showScope=false; break;
        case 2: anAction=::AdminActionDialog_Ban_String; showDuration=true; enableReason=true; this->mIsReasonOptional=false; showScope=true; break;
    }
    this->mDurationHeader->SetVisible(showDuration);
    this->mDurationCombo->SetVisible(showDuration);
    this->mTimeSegmentCombo->SetVisible(showDuration);
    this->mApplyToServer->SetVisible(showScope);
    this->mApplyToRoom->SetVisible(showScope);
    this->mExplanation->SetEnabled(enableReason);
    if(enableReason) this->mExplanation->RequestFocus();
    this->UpdateDoActionButton();
    GUIString aTitle=anAction;
    aTitle.append(GUIString(new GUIStringData(" ")));
    aTitle.append(inTarget->GetName());
    this->mTitleLabel->SetText(aTitle,true);
    if(this->mIsReasonOptional) this->mExplanationHeader->SetText(::AdminActionDialog_ExplanationOpt_String,true);
    else this->mExplanationHeader->SetText(::AdminActionDialog_ExplanationReq_String,true);
    this->mDoActionButton->SetText(anAction,true);
    this->Relayout();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f4ab0
// (public: virtual void * __thiscall WONAPI::AdminActionDialog::`scalar deleting destructor'(unsigned int)    [??_GAdminActionDialog@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f4a60
// (public: __thiscall WONAPI::AdminActionDialog::AdminActionDialog(void)    [??0AdminActionDialog@WONAPI@@QAE@XZ])
