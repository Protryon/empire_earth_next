// C:\EEX\Empire Earth\Empire Earth\EEVoting.cpp (name from RTTI EEVoting; no __FILE__). 0x137b3f..0x137ba3: the
// edict vote the world holds at +0xb5c (ctor, ??_G, dtor, Propose). EEUnitSetManager.cpp ends at 0x137b3f,
// EEWorld.cpp starts at 0x137ba3. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEVoting.h"

// FUNCTION: EXE 0x537b3f
// Creates an empty edict vote with no edict proposed
EEVoting::EEVoting()
{
    this->mU06 = this->mEdict = 0;
    this->mB08 = this->mB0A = false;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x537b58

// FUNCTION: EXE 0x537b74
// Destroys the edict vote
EEVoting::~EEVoting()
{
}

// FUNCTION: EXE 0x537b7b
// Proposes an edict for a vote unless one is already pending; returns 0 when accepted, 1 when busy
long EEVoting::Propose(unsigned short inEdict, long inArg2)
{
    long theResult = 1;
    if (this->mEdict == 0) {
        this->mU06 = (short)inArg2;
        this->mEdict = inEdict;
        this->mB08 = this->mB0A = false;
        theResult = 0;
    }
    return theResult;
}
