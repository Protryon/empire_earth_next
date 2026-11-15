// C:\EEX\Empire Earth\AI Thread\EECPPlayerInteractionManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPPlayerInteractionManager, vtable 0x43c270). A computer player's chat/taunt manager: 0x89574..0x89fc4 (the entry
// struct's out-of-line members first: only this unit calls them; the map<long, EECPPIMEntry> COMDATs close the unit;
// EECPResearchManager's key chain starts at 0x89fc4).
#include <windows.h>
#include <vector>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPPlayerInteractionManager.h"

// FUNCTION: EXE 0x489574
// Creates a copy of a chat history entry
EECPPIMEntry::EECPPIMEntry(const EECPPIMEntry& inEntry)
{
    this->Copy(inEntry);
}

// FUNCTION: EXE 0x489586
// Zeroes every field of the chat history entry
void EECPPIMEntry::Clear()
{
    this->mTime = 0;
    this->mPlayer = 0;
    this->mL00 = 0;
    this->mL0C = 0;
    this->mL10 = 0;
    this->mL14 = 0;
    this->mL18 = 0;
    this->mL1C = 0;
    this->mL20 = 0;
}

EECPPIMEntry& EECPPIMEntry::operator=(const EECPPIMEntry& inEntry)
{
    this->Copy(inEntry);
    return *this;
}

// FUNCTION: EXE 0x4895a3
// Copies every field of another chat history entry
void EECPPIMEntry::Copy(const EECPPIMEntry& inEntry)
{
    this->mTime = inEntry.mTime;
    this->mL0C = inEntry.mL0C;
    this->mL10 = inEntry.mL10;
    this->mL14 = inEntry.mL14;
    this->mL18 = inEntry.mL18;
    this->mL1C = inEntry.mL1C;
    this->mL20 = inEntry.mL20;
    this->mPlayer = inEntry.mPlayer;
    this->mL00 = inEntry.mL00;
}

// FUNCTION: EXE 0x4895de
// Creates the computer player's chat manager, seeded from the player, with no line used yet in any of 8 groups
EECPPlayerInteractionManager::EECPPlayerInteractionManager(EEComputerPlayer* inComputerPlayer)
{
    this->mComputerPlayer = inComputerPlayer;
    this->mIndexSeed = inComputerPlayer->mSeed;
    this->mB28 = true;
    this->mLastIndex.resize(8, -1);
}

// SYNTHETIC: EXE 0x489635
// (the scalar deleting destructor)

// FUNCTION: EXE 0x489651
// Destroys the chat manager (members only)
EECPPlayerInteractionManager::~EECPPlayerInteractionManager()
{
}

// FUNCTION: EXE 0x48966c
// Unless the message was sent less than 5 s ago, picks one of its 10 string variants (never the group's last one)
// and posts it to the chat log; returns whether it was posted
bool EECPPlayerInteractionManager::SendChat(long inStringID, long inType)
{
    UWideString theString;
    bool theResult = false;
    std::map<long, EECPPIMEntry>::iterator theEntry = this->mEntries.find(inStringID);
    bool theReady = true;
    if (theEntry != this->mEntries.end())
        theReady = theEntry->second.mTime < gPhysics->mGameTime;
    if (theReady) {
        long theGroup;
        switch (inStringID) {
        case 12500:
            theGroup = 5;
            break;
        case 12400:
        case 12430:
            theGroup = 4;
            break;
        case 12300:
        case 12330:
        case 12360:
        case 12390:
            theGroup = 3;
            break;
        case 12100:
        case 12200:
            theGroup = 2;
            break;
        case 12000:
            theGroup = 1;
            break;
        default:
            theGroup = 0;
            break;
        }
        long theIndex = (gPhysics->mGameTime + this->mIndexSeed) % 10;
        if (theGroup < this->mLastIndex.size() && this->mLastIndex[theGroup] == theIndex)
            theIndex = (theIndex + 1) % 10;
        if (theGroup < this->mLastIndex.size())
            this->mLastIndex[theGroup] = theIndex;
        gResource->GetString(inStringID + theIndex, theString);
        if (theString.GetLength() > 0)
            theResult = this->SendChat(inStringID, inType, theString);
    }
    return theResult;
}

// FUNCTION: EXE 0x48979b
// Posts a chat line under the computer player's name and colour and blocks the message id for 5 s; returns true
bool EECPPlayerInteractionManager::SendChat(long inStringID, long inType, const UWideString& inString)
{
    EECPPIMEntry theEntry;
    if (!(inString.GetLength() > 0))
        return true;
    gUserInterface->AddChatLogEntry(this->mComputerPlayer->mColor, this->mComputerPlayer->mName, inString, inType, 0);
    theEntry.mPlayer = gWorld.mCurrentPlayer;
    theEntry.mTime = gPhysics->mGameTime + 5000;
    this->mEntries[inStringID] = theEntry;
    return true;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt): the std::map<long, EECPPIMEntry> COMDATs
// TEMPLATE: EXE 0x48980c
// TEMPLATE: EXE 0x48984a
// TEMPLATE: EXE 0x489875
// TEMPLATE: EXE 0x4898af
// TEMPLATE: EXE 0x489965
// TEMPLATE: EXE 0x4899d6
// TEMPLATE: EXE 0x489c84
// TEMPLATE: EXE 0x489cb6
// TEMPLATE: EXE 0x489ceb
// TEMPLATE: EXE 0x489e7f
// TEMPLATE: EXE 0x489ea8
// TEMPLATE: EXE 0x489eea
// TEMPLATE: EXE 0x489f2e
// TEMPLATE: EXE 0x489f6f
// TEMPLATE: EXE 0x489fa7
