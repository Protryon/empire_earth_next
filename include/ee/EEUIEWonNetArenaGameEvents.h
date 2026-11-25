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
// ProcessEvent made its one call of the Rust front end (ee_won_lobby wonfront.rs); they are one class holding the call.
// Back and the two Cancels pop the manager's form (the original's three identical bodies, ICF-folded).
struct EEWonFrontEvent : UIEvent {
    explicit EEWonFrontEvent(EeWonFrontCall inCall) : mCall(inCall) {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(mCall, inForm, (intptr_t)inEntity); }
    EeWonFrontCall mCall;
};
struct EEUIEWNPop : UIEvent {
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return inForm->GetManager()->Pop(); }
};
// FrontNewEvent's event (ee_won_front_event_call: the event's call, or -1 for a pop)
inline UIEvent* EEWonFrontNewEvent(int32_t inCall)
{
    return inCall < 0 ? static_cast<UIEvent*>(new EEUIEWNPop) : new EEWonFrontEvent((EeWonFrontCall)inCall);
}

#endif
