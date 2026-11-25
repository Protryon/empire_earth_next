// The WON front end's screens' event (R6 WON lobby and network round 37): the one class of the events the game
// room, room list, account, server select and statistics forms run. Each was a class of its own
// (EEUIEWNAGActivateForm .. EEUIEWNSOpen, 30 classes in EEUIEWonNet{ArenaGame,ArenaRoom,ModifyUserAccount,
// ServerSelect,Stats}Events.h) whose ProcessEvent forwarded its EeWonFrontCall to the Rust screens (ee_won_lobby
// wonfront.rs); the call is now the event's member. Requires lle/UIEvent.h.
#ifndef EE_EEUIEWONNETEVENT_H
#define EE_EEUIEWONNETEVENT_H

#include "rust/won_lobby/wonfront.h"

class EEUIEWonNetEvent : public UIEvent {
public:
    explicit EEUIEWonNetEvent(EeWonFrontCall inCall) : mCall(inCall) {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(this->mCall, inForm, (intptr_t)inEntity); }
    EeWonFrontCall mCall;
};

#endif
