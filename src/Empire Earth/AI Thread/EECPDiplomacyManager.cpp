// C:\EEX\Empire Earth\AI Thread\EECPDiplomacyManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPDiplomacyManager, vtable 0x43c1a4). A computer player's (empty) diplomacy manager: 0x7ed03..0x7ed38 (the
// previous unit's map/set COMDATs end at 0x7ed03, EECPExplorationManager's ctor starts at 0x7ed38).
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EECPDiplomacyManager.h"

// Creates the computer player's diplomacy manager, remembering its owner
EECPDiplomacyManager::EECPDiplomacyManager(EEComputerPlayer* inComputerPlayer)
{
    this->mComputerPlayer = inComputerPlayer;
}

// Destroys the diplomacy manager (nothing to release)
EECPDiplomacyManager::~EECPDiplomacyManager()
{
}

// Does nothing each AI tick. (No address: EEComputerPlayer::Update 0x43f175 calls 0x557269, the ICF representative
// of an identical empty `ret 4`, so this body was folded away; defined here, its class's unit, for the link.)
void EECPDiplomacyManager::Update(unsigned long inTime)
{
}
