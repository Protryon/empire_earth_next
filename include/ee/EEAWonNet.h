// C:\EEX\Empire Earth\Actions\EEAWonNet.h (RECONSTRUCTION; file name inferred). The WON lobby's
// UI-thread action (RTTI EEAWonNet > EEAction; vtable 0x43914c): EEWonNet posts one per lobby callback and Virtual4
// feeds the WON forms (room list 0x1f, game room 0x20).
// R6 WON lobby and network round 37: the class is the Rust action's shell (ee_won_lobby wonaction.rs: the members,
// the three constructors, Virtual4 and its statics); EEWonNet (wonnet.rs) makes the action, the host's NewWonAction
// wraps it.
#ifndef EE_EEAWONNET_H
#define EE_EEAWONNET_H

#include "ee/EEAction.h"
#include "rust/won_lobby/wonaction.h"

class EEAWonNet : public EEAction {
public:
    explicit EEAWonNet(EeWonAction* inRust) : mRust(inRust) {}     // takes the action
    virtual ~EEAWonNet() { ee_won_action_free(this->mRust); }
    virtual void Virtual4();                // the action's run over EEAWonNet.cpp's host
    virtual void Virtual8() {}              // +0x08 (the shared empty slot 0x1720c6)

    EeWonAction* mRust;                     // owned
};

#endif
