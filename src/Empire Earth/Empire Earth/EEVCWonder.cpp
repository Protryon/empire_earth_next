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
extern bool& gInCinematic;                   // 0x5318e0 (no wonder victory while set; provisional name)

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

// ------------------------------------------------------------------------- round 10, lane D: the wonder victory

int64_t EECore10OpD7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameUtilities.cpp
double EECore10OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define VC ((EEVictoryCondition*)p)

// the C++ calls and members of the wonder victory's bodies in Rust (gamecpp civilization.rs): p the victory
// condition unless noted, one statement each
int64_t EECore10OpD6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DwTeamCount: return VC->mTeam->GetNumPlayers();
    case O::DwTeamPlayer: return VC->mTeam->mPlayers[a[0]];
    case O::DwNeeded: return gWorld.mNumberOfWondersToWin;
    case O::DwWonderCount: return ((EEPlayer*)p)->GetNumWonderUnits();
    case O::DwWonderTime: return ((EEPlayer*)p)->GetWonderTime((long)a[0]);
    case O::DwVictoryTime: return gWorldTable.GetLongValue(kWorldWonderVictoryTime);
    case O::DwGameTime: return gPhysics->mGameTime;
    case O::DwInCinematic: return gInCinematic;
    default: return EECore10OpD7(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpD6F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpD7F(inOp, p, a, f);
}
