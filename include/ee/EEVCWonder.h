// EEVCWonder.h (RECONSTRUCTION; file name inferred). The wonder victory condition (RTTI EEVCWonder >
// EEVictoryCondition, vtable 0x442064, sizeof 0xc): EETeam's ctor 0x13d013 news it with the team (inline ctor: +4 =
// 0, +8 = the team); ??_G 0x13d059. EEVCWonder.cpp 0x1372a6..0x137b3f. Requires ee/EEVictoryCondition.h.
#ifndef EE_EEVCWONDER_H
#define EE_EEVCWONDER_H

#include "rust/game/civilization.h"   // ee_gamecpp_wonder_* (ee_game civilization.rs)
class EEVCWonder : public EEVictoryCondition {
public:
    EEVCWonder(EETeam* inTeam) : EEVictoryCondition(inTeam) {}   // inline (EETeam's ctors 0x13d013..)
    // Returns 1 (won) when the team's wonder countdown has run out, outside cinematics; else 0
    // +0x04 0x1374c1 (1: a wonder has stood long enough)
    virtual long GetVictoryState() { return ee_gamecpp_wonder_state(this); }
    // Reads and discards the wonder list of old saved games (before version 0.225); returns the first read error
    // +0x08 0x13744b
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_wonder_load(inMajorVersion, inMinorVersion, &inFile); }
};

#endif
