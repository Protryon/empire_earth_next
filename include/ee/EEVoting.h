// C:\EEX\Empire Earth\Empire Earth\EEVoting.h (RECONSTRUCTION; file name inferred). The edict vote the world holds
// at +0xb5c (RTTI EEVoting, vtable 0x441fdc, sizeof 0xc; EEVoting.cpp 0x137b3f..0x137ba3: ctor, ??_G, dtor, Propose).
// The members and Propose are Rust's (ee_sim app voting.rs); the port's class has no vtable (nothing deletes it
// through a pointer). Member names are provisional.
#ifndef EE_EEVOTING_H
#define EE_EEVOTING_H

#include "rust/sim/app.h"

class EEVoting : public EeAppVoting {
public:
    EEVoting() : EeAppVoting{} {}          // 0x137b3f: every member 0 (the original left +0x09)
    // 0x137b7b (1: a proposal is already open) (EEMEdictProposal 0xbde48; provisional name)
    long Propose(unsigned short inEdict, long inArg2) { return ee_app_voting_propose(this, inEdict, inArg2); }
};

#endif
