// C:\EEX\Empire Earth\Empire Earth\EETeam.cpp (name from RTTI EETeam; no __FILE__). 0x136b6d..0x136d66: a team's
// players (Merge, AddPlayer, RemovePlayer, Contains, Load, Save, GetVictoryState). The ctor 0x13d013 and dtor 0x131b02
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

// FUNCTION: EXE 0x536b6d
// Adds every player of another team to this team
void EETeam::Merge(const EETeam& inTeam)
{
    for (unsigned long i = 0; i < inTeam.mPlayers.size(); i++)
        this->AddPlayer(inTeam.mPlayers[i]);
}

// FUNCTION: EXE 0x536ba1
// Adds a player to the team unless already a member
void EETeam::AddPlayer(unsigned long inPlayer)
{
    if (std::find(this->mPlayers.begin(), this->mPlayers.end(), inPlayer) == this->mPlayers.end())
        this->mPlayers.push_back(inPlayer);
}

// FUNCTION: EXE 0x536bfc
// Returns a copy of the team's players without the given player
void EETeam::RemovePlayer(long inPlayer, std::vector<unsigned long>& outPlayers)
{
    outPlayers = this->mPlayers;
    std::vector<unsigned long>::iterator it = std::find(outPlayers.begin(), outPlayers.end(), inPlayer);
    if (it != outPlayers.end())
        outPlayers.erase(it);
}

// FUNCTION: EXE 0x536c64
// Returns whether both players belong to the team
bool EETeam::Contains(long inPlayer1, long inPlayer2)
{
    std::vector<unsigned long>::iterator theEnd = this->mPlayers.end();
    std::vector<unsigned long>::iterator it = this->mPlayers.begin();
    while (true) {
        if (it == theEnd || *it == inPlayer1)
            break;
        it++;
    }
    bool theFound = it != theEnd;
    if (!theFound)
        return theFound;
    it = this->mPlayers.begin();
    while (true) {
        if (it == theEnd || *it == inPlayer2)
            break;
        it++;
    }
    theFound = it != theEnd;
    return theFound;
}

// FUNCTION: EXE 0x536ca3
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

// FUNCTION: EXE 0x536d03
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

// FUNCTION: EXE 0x536d5e
// Returns the team's victory state from its victory condition
long EETeam::GetVictoryState()
{
    return this->mVictoryCondition->GetVictoryState();
}
