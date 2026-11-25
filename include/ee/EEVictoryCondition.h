// EEVictoryCondition.h (RECONSTRUCTION; file name inferred). A team's victory condition
// (RTTI EEVictoryCondition; EETeam +0x14 news an EEVCWonder, vtable 0x442064). Only the member EECPIntelligence.cpp
// calls is declared; the slots and layout are not decoded yet. Names are provisional.
#ifndef EE_EEVICTORYCONDITION_H
#define EE_EEVICTORYCONDITION_H

#include "rust/game/gamecpp.h"
class FSFile;
class EEPlayer;
class EETeam;

class EEVictoryCondition {
public:
    // inline (EETeam's ctors 0x13d013..: +4/+8 stored before the derived vptr)
    EEVictoryCondition(const EETeam* inTeam) : mL04(0), mTeam(inTeam) {}
    // slots from EEVCWonder's vtable 0x442064 (provisional names)
    virtual ~EEVictoryCondition() {}                               // +0x00 (EEVCWonder ??_G 0x13d059 stores the base vptr, no dtor call)
    // pure: the base vtable 0x442074 is {??_G 0x13d059, _purecall}; Load is EEVCWonder's own
    virtual long GetVictoryState() = 0;                            // +0x04 (EEVCWonder 0x1374c1; EETeam 0x136d5e tail-calls it)
    // 0x1372f9 (ret 4): non-zero when the condition has a progress value, stored in outValue (EECPIntelligence 0x8126c
    // divides it by gWorldTable entry 0x22's +0x10 for a percentage)
    // Returns the player whose wonder completes the team's winning count and the seconds left of the wonder
    // countdown (NULL and 0 when the team lacks the wonders)
    EEPlayer* GetWonderCountdown(unsigned long& outValue) { return static_cast<EEPlayer*>(ee_gamecpp_wonder_countdown(this, &outValue)); }
    // Returns whether the team's players together own as many wonders as the game needs to win (never when none)
    // 0x1372a6 (EEComputerPlayer::WorkOutPopulation 0x2d1c7 tests al) (provisional name)
    bool HasEnoughWonders() { return ee_gamecpp_wonder_enough(this); }

    long mL04;                              // +0x04 (0 from EETeam's ctor 0x13d013) (provisional name)
    const EETeam* mTeam;                    // +0x08 (the owning team: EETeam's ctor 0x13d013; HasEnoughWonders/GetWonderCountdown read it)
};

#endif
