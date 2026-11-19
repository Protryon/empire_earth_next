// C:\EEX\Empire Earth\Empire Earth\EETeam.cpp (name from RTTI EETeam; no __FILE__). 0x136b6d..0x136d66: a team's
// players (Load, Save, GetVictoryState; Merge, AddPlayer, RemovePlayer and Contains are ee_sim app teams.rs). The ctor 0x13d013 and dtor 0x131b02
// are COMDATs in other units' ranges (inline in the real header); GetChecksum 0x156dd8 is an ICF-folded empty body.
// EEPreferences.cpp ends at 0x136b6d, EEUnitSet.cpp starts at 0x136d66 (provisional names).
#include <windows.h>
#include <algorithm>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"

#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"

// Reads the team's players and its victory condition from a saved game; returns the first read error
long EETeam::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    // a failed count read leaves no count to size the player list by
    if (theResult != kErrorOK)
        return theResult;
    this->mPlayers.resize(theCount, 0);
    for (unsigned long i = 0; i < theCount; i++) {
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPlayers[i]);
    }
    // the player reads' error is reported (it used to be discarded)
    if (theResult != kErrorOK)
        return theResult;
    return this->mVictoryCondition->Load(inMajorVersion, inMinorVersion, inFile);
}

// Writes the team's players to a saved game; returns the first write error
long EETeam::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = this->mPlayers.size();
    long theResult = outFile.Write(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        if (theResult == 0)
            theResult = outFile.Write(this->mPlayers[i]);
    }
    return theResult;
}

// Returns the team's victory state from its victory condition
long EETeam::GetVictoryState()
{
    return this->mVictoryCondition->GetVictoryState();
}

void EETeam::GetChecksum(class ESChecksum &) const
{
}
