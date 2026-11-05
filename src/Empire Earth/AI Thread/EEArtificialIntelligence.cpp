// C:\EEX\Empire Earth\AI Thread\EEArtificialIntelligence.cpp (file name inferred: no __FILE__; RTTI class
// EEArtificialIntelligence, vtable 0x44218c). The computer players' scheduler: 0x2cc87..0x2ce2e (EEAIRectangleArea's
// ??_G ends at 0x2cc87, EEComputerPlayer.cpp's key chain starts at 0x2ce2e). Two free helpers, the members, then
// vector<EEComputerPlayer*>::push_back (first used here).
#include <windows.h>
#include <vector>
#include <chrono>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EECPTrace.h"

// FUNCTION: EXE 0x42cc87
// Returns a unit type's weight: 3 for the space fighter, 1 otherwise
long EEAIGetUnitTypeWeight(long inUnitType)
{
    return inUnitType == kUnitTypeSp15SpaceFighter ? 3 : 1;
}

// FUNCTION: EXE 0x42cc9a
// Returns the player's first unit of the type whose class transport capacity over its complex unit's, plus
// its cargo room, is nonzero; NULL when none
EEUnit* EEAIFindUpgradableUnit(long inUnitType, EEPlayer* inPlayer)
{
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(inUnitType, inPlayer->GetPlayerID(), theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); ++it) {
        EEUnit* theUnit = *it;
        if (theUnit->mUnitClass->mTransportCapacity - theUnit->mComplexUnit->mUnitClass->mTransportCapacity + theUnit->mComplexUnit->mCargoRoom != 0)
            return theUnit;
    }
    return NULL;
}

// FUNCTION: EXE 0x42cd0e
// Lists every player's computer player and spreads their updates over a second (1000 ms / count), starting
// with the first at time 0
void EEArtificialIntelligence::CollectComputerPlayers()
{
    this->mComputerPlayers.clear();
    for (unsigned long i = 0; i < ::gWorld.mNumPlayers; i++) {
        EEComputerPlayer* theComputerPlayer = ::gWorld.mPlayers[i]->mComputerPlayer;
        if (theComputerPlayer)
            this->mComputerPlayers.push_back(theComputerPlayer);
    }
    if (this->mComputerPlayers.size() > 0)
        this->mUpdateInterval = 1000 / this->mComputerPlayers.size();
    this->mNextPlayer = 0;
    this->mNextUpdateTime = 0;
}

// FUNCTION: EXE 0x42cd99
// When the next update time is reached, updates the next active computer player in turn and schedules the
// following one an interval later
void EEArtificialIntelligence::Update()
{
    unsigned long theTime = gPhysics->mGameTime;
    if (gCPTraceOn && this->mComputerPlayers.size() > 0)
        EECPTraceHash(theTime);
    if (!(theTime >= this->mNextUpdateTime) || this->mComputerPlayers.size() <= 0)
        return;
    if (this->mComputerPlayers[this->mNextPlayer]->mActive) {
        if (gCPTraceOn) {   // the AI's real-time cost (EE-AIPROF lines; never fed back into the simulation)
            std::chrono::steady_clock::time_point theStart = std::chrono::steady_clock::now();
            this->mComputerPlayers[this->mNextPlayer]->Update(theTime);
            long long theMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - theStart).count();
            EECPTraceProfile(this->mComputerPlayers[this->mNextPlayer], theTime, theMicroseconds);
        } else
            this->mComputerPlayers[this->mNextPlayer]->Update(theTime);
    }
    this->mNextPlayer++;
    if (this->mNextPlayer == this->mComputerPlayers.size())
        this->mNextPlayer = 0;
    this->mNextUpdateTime = this->mUpdateInterval + theTime;
}

// TEMPLATE: EXE 0x42ce02
// std::vector<EEComputerPlayer*>::push_back (first used by CollectComputerPlayers)
