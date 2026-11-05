// C:\EEX\Empire Earth\Empire Earth\EEVCWonder.cpp (file name inferred: no __FILE__; the class EEVCWonder, vtable
// 0x442064). 0x1372a6..0x137b3f: EEUnitSetManager.cpp ends at 0x1372a6 and EEVoting.cpp starts at 0x137b3f
// (alphabetical). The victory condition's wonder count and countdown (members declared on EEVictoryCondition, called
// through it), EEVCWonder's Load and GetVictoryState, then the vector and sort COMDATs of the wonder entries.
// Unit flags: the exe module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EETeam.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"

extern EEWorld gWorld;
extern EEPhysics* gPhysics;
extern bool& gInCinematic;                   // 0x5318e0 (no wonder victory while set; provisional name)

// one standing wonder: its player and when it was built; ordered by the time, then the player (provisional names)
struct EEVCWonderEntry {
    unsigned long mPlayer;
    unsigned long mTime;
    bool operator<(const EEVCWonderEntry& inOther) const
    {
        return mTime < inOther.mTime || (mTime == inOther.mTime && mPlayer < inOther.mPlayer);
    }
};

// FUNCTION: EXE 0x5372a6
// Returns whether the team's players together own as many wonders as the game needs to win (never when none)
bool EEVictoryCondition::HasEnoughWonders()
{
    unsigned long theNeeded = gWorld.mNumberOfWondersToWin;
    bool theResult = false;
    if (theNeeded > 0) {
        unsigned long theCount = 0;
        for (unsigned long i = 0; i < this->mTeam->GetNumPlayers(); i++)
            theCount += gWorld.mPlayers[this->mTeam->mPlayers[i]]->GetNumWonderUnits();
        theResult = theCount >= theNeeded;
    }
    return theResult;
}

// FUNCTION: EXE 0x5372f9
// Returns the player whose wonder completes the team's winning count and the seconds left of the wonder
// countdown (NULL and 0 when the team lacks the wonders)
EEPlayer* EEVictoryCondition::GetWonderCountdown(unsigned long& outValue)
{
    std::vector<EEVCWonderEntry> theEntries;
    unsigned long theNeeded = gWorld.mNumberOfWondersToWin;
    EEPlayer* theWinner;
    if (this->HasEnoughWonders() == true) {
        for (unsigned long i = 0; i < this->mTeam->GetNumPlayers(); i++) {
            EEPlayer* thePlayer = gWorld.mPlayers[this->mTeam->mPlayers[i]];
            for (unsigned long j = 0; j < thePlayer->GetNumWonderUnits() && j < theNeeded; j++) {
                EEVCWonderEntry theEntry;
                theEntry.mPlayer = thePlayer->mPlayerID;
                theEntry.mTime = thePlayer->GetWonderTime(j);
                theEntries.push_back(theEntry);
            }
        }
        std::sort(theEntries.begin(), theEntries.end());
        EEVCWonderEntry& theLast = theEntries[theNeeded - 1];
        theWinner = gWorld.mPlayers[theLast.mPlayer];
        long theLeft = gWorldTable.GetLongValue(kWorldWonderVictoryTime) - (gPhysics->mGameTime - theLast.mTime) / 1000;
        if (theLeft <= 0)
            theLeft = 0;
        outValue = theLeft;
    } else {
        theWinner = NULL;
        outValue = 0;
    }
    return theWinner;
}

// FUNCTION: EXE 0x53744b
// Reads and discards the wonder list of old saved games (before version 0.225); returns the first read error
long EEVCWonder::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    std::vector<unsigned long> theOld;
    unsigned long theCount;
    long theResult = kErrorOK;
    if (inMajorVersion == 0 && inMinorVersion < 0xe1) {
        theResult = inFile.Read(theCount);
        theOld.resize(theCount, 0);
        for (unsigned long i = 0; i < theCount; i++)
            if (theResult == kErrorOK)
                theResult = inFile.Read(theOld[i]);
    }
    return theResult;
}

// FUNCTION: EXE 0x5374c1
// Returns 1 (won) when the team's wonder countdown has run out, outside cinematics; else 0
long EEVCWonder::GetVictoryState()
{
    long theState = 0;
    if (!gInCinematic) {
        unsigned long theValue;
        if (this->GetWonderCountdown(theValue) != NULL && theValue == 0)
            theState = 1;
    }
    return theState;
}

// TEMPLATE: EXE 0x5374eb
// ?push_back@?$vector@UEEVCWonderEntry@@V?$allocator@UEEVCWonderEntry@@@std@@@std@@QAEXABUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x537517
// ?sort@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0@Z
// TEMPLATE: EXE 0x5375c5
// ?_Insertion_sort@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0@Z
// TEMPLATE: EXE 0x5375ea
// ?_Sort@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0H@Z
// TEMPLATE: EXE 0x5376ae
// ?_Unguarded_insertion_sort@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0PAUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x5376df
// ?_Insertion_sort_1@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0PAUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x537787
// ?make_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0@Z
// TEMPLATE: EXE 0x5377be
// ?sort_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0@Z
// TEMPLATE: EXE 0x5377f1
// ?_Unguarded_partition@std@@YA?AV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@V21@0PAUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x53790c
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@UEEVCWonderEntry@@@Z

// TEMPLATE: EXE 0x537942
// ?_Make_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0PAHPAUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x53798a
// ?pop_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0@Z
// TEMPLATE: EXE 0x5379af
// ?_Adjust_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@HHUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x537a38
// ?_Pop_heap_0@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0PAUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x537a8c
// ?_Iter_swap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@0PAUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x537aad
// ?_Push_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@HHUEEVCWonderEntry@@@Z
// TEMPLATE: EXE 0x537b05
// ?_Pop_heap@std@@YAXV?$_Ptrit@UEEVCWonderEntry@@HPAU1@AAU1@PAU1@AAU1@@1@00UEEVCWonderEntry@@PAH@Z
