// C:\EEX\Empire Earth\Empire Earth\EEVCWonder.cpp (file name inferred: no __FILE__; the class EEVCWonder, vtable
// 0x442064). 0x1372a6..0x137b3f: EEUnitSetManager.cpp ends at 0x1372a6 and EEVoting.cpp starts at 0x137b3f
// (alphabetical). The victory condition's wonder count and countdown (members declared on EEVictoryCondition, called
// through it), EEVCWonder's Load and GetVictoryState, then the vector and sort COMDATs of the wonder entries.
// Unit flags: the exe module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EETeam.h"
#include "ee/EEDatabaseTables.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp civilization.rs)

extern EEWorld gWorld;
extern EEPhysics* gPhysics;

// (the wonder entries, their sort and the countdown: ee_game gamecpp civilization.rs)

// Returns whether the team's players together own as many wonders as the game needs to win (never when none)
bool EEVictoryCondition::HasEnoughWonders()
{
    return ee_gamecpp_wonder_enough(this);
}

// Returns the player whose wonder completes the team's winning count and the seconds left of the wonder
// countdown (NULL and 0 when the team lacks the wonders)
EEPlayer* EEVictoryCondition::GetWonderCountdown(unsigned long& outValue)
{
    return static_cast<EEPlayer*>(ee_gamecpp_wonder_countdown(this, &outValue));
}

// Reads and discards the wonder list of old saved games (before version 0.225); returns the first read error
long EEVCWonder::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_wonder_load(inMajorVersion, inMinorVersion, &inFile);
}

// Returns 1 (won) when the team's wonder countdown has run out, outside cinematics; else 0
long EEVCWonder::GetVictoryState()
{
    return ee_gamecpp_wonder_state(this);
}

// (round 10, lane D: the wonder victory's cases are EEGameLog.cpp's EECore11OpB, round 11 lane B)
