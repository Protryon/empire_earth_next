// C:\EEX\Empire Earth\UI Events\EEUIEWonNetArenaGameEvents.h (RECONSTRUCTION; header and file names inferred,
// from the class prefix EEUIEWNAG). The WON game room form's (0x20) events.
// ProcessEvents: EEUIEWonNetArenaGameEvents.cpp (0x1e9fed..0x1eafcb). Declaration order is alphabetical: the order of
// the ??_G VC6 emits after OpenScreen (UpdateForm's comes after ActivateForm, its first user).
// Requires lle/UIEvent.h.
#ifndef EE_EEUIEWONNETARENAGAMEEVENTS_H
#define EE_EEUIEWONNETARENAGAMEEVENTS_H

#include "lle/LLEForward.h"
#include "rust/won_lobby/wonfront.h"

// 0x1eadd6: sets inEvent as inControl's trigger event and drops the creator's reference. Name is provisional. A static
// (non-inline) function of this header: every WON screen's OpenScreen calls it out of line, and WNAG's copy sits right
// after its first user (OpenScreen 0x1ea884), before the ??_G block, which is where VC6 emits a deferred static; an
// external function defined after OpenScreen lands after the ??_G block, an inline one is expanded. The copies of the
// other WON units are ICF-folded into WNAG's. Needs lle/UIControl.h and lle/UIEvent.h first.
static void EEUIEWNSetTriggerEvent(UIEvent* inEvent, UIControl* inControl, unsigned long inTrigger)
{
    inControl->SetTriggerEvent(inTrigger, inEvent);
    inEvent->Dereference();
}

// The WON screens' events (the game room form 0x20's EEUIEWNAG*, the room list's EEUIEWNAR*, the update-account form
// 0x25's EEUIEWNMUA*, the server list's EEUIEWNSS*, the statistics form's EEUIEWNS*): each was a class whose
// ProcessEvent made its one call of the Rust front end (ee_won_lobby wonfront.rs); they are one template by the call.
// Back and the two Cancels pop the manager's form (the original's three identical bodies, ICF-folded).
template <EeWonFrontCall kCall> struct EEWonFrontEvent : UIEvent {
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(kCall, inForm, (intptr_t)inEntity); }
};
struct EEUIEWNPop : UIEvent {
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return inForm->GetManager()->Pop(); }
};
typedef EEUIEWNPop EEUIEWNARBack, EEUIEWNMUACancel, EEUIEWNSSCancel;
typedef EEWonFrontEvent<EeWonFrontCall::AgActivateForm> EEUIEWNAGActivateForm;
typedef EEWonFrontEvent<EeWonFrontCall::AgBack> EEUIEWNAGBack;
typedef EEWonFrontEvent<EeWonFrontCall::AgCheckPassword> EEUIEWNAGCheckPassword;
typedef EEWonFrontEvent<EeWonFrontCall::AgClickGameFilter> EEUIEWNAGClickGameFilter;
typedef EEWonFrontEvent<EeWonFrontCall::AgCloseForm> EEUIEWNAGCloseForm;
typedef EEWonFrontEvent<EeWonFrontCall::AgDeactivateForm> EEUIEWNAGDeactivateForm;
typedef EEWonFrontEvent<EeWonFrontCall::AgDoubleclickUser> EEUIEWNAGDoubleclickUser;
typedef EEWonFrontEvent<EeWonFrontCall::AgHostOrJoinGame> EEUIEWNAGHostOrJoinGame;
typedef EEWonFrontEvent<EeWonFrontCall::AgOpenScreen> EEUIEWNAGOpenScreen;
typedef EEWonFrontEvent<EeWonFrontCall::AgSendChat> EEUIEWNAGSendChat;
typedef EEWonFrontEvent<EeWonFrontCall::AgToggleHostScreen> EEUIEWNAGToggleHostScreen;
typedef EEWonFrontEvent<EeWonFrontCall::AgToggleJoinScreen> EEUIEWNAGToggleJoinScreen;
typedef EEWonFrontEvent<EeWonFrontCall::AgUpdateForm> EEUIEWNAGUpdateForm;
typedef EEWonFrontEvent<EeWonFrontCall::ArUpdateForm> EEUIEWNARUpdateForm;
typedef EEWonFrontEvent<EeWonFrontCall::ArActivateForm> EEUIEWNARActivateForm;
typedef EEWonFrontEvent<EeWonFrontCall::ArDeactivateForm> EEUIEWNARDeactivateForm;
typedef EEWonFrontEvent<EeWonFrontCall::ArDoubleClickRoom> EEUIEWNARDoubleClickRoom;
typedef EEWonFrontEvent<EeWonFrontCall::ArClickUpdateAccount> EEUIEWNARClickUpdateAccount;
typedef EEWonFrontEvent<EeWonFrontCall::ArJoinRoom> EEUIEWNARJoinRoom;
typedef EEWonFrontEvent<EeWonFrontCall::ArOpenScreen> EEUIEWNAROpenScreen;
typedef EEWonFrontEvent<EeWonFrontCall::MuaOpenScreen> EEUIEWNMUAOpenScreen;
typedef EEWonFrontEvent<EeWonFrontCall::MuaSubmit> EEUIEWNMUASubmit;
typedef EEWonFrontEvent<EeWonFrontCall::SsOk> EEUIEWNSSOk;
typedef EEWonFrontEvent<EeWonFrontCall::SsOpenScreen> EEUIEWNSSOpenScreen;
typedef EEWonFrontEvent<EeWonFrontCall::StatsCancel> EEUIEWNSCancel;
typedef EEWonFrontEvent<EeWonFrontCall::StatsOpen> EEUIEWNSOpen;

#endif
