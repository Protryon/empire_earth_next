// C:\EEX\Empire Earth\AI Thread\EECPDiplomacyManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPDiplomacyManager, vtable 0x43c1a4). A computer player's (empty) diplomacy manager: 0x7ed03..0x7ed38 (the
// previous unit's map/set COMDATs end at 0x7ed03, EECPExplorationManager's ctor starts at 0x7ed38).
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EECPDiplomacyManager.h"

// FUNCTION: EXE 0x47ed03
// Creates the computer player's diplomacy manager, remembering its owner
EECPDiplomacyManager::EECPDiplomacyManager(EEComputerPlayer* inComputerPlayer)
{
    this->mComputerPlayer = inComputerPlayer;
}

// SYNTHETIC: EXE 0x47ed15
// (the scalar deleting destructor)

// FUNCTION: EXE 0x47ed31
// Destroys the diplomacy manager (nothing to release)
EECPDiplomacyManager::~EECPDiplomacyManager()
{
}

// Does nothing each AI tick. (No address: EEComputerPlayer::Update 0x43f175 calls 0x557269, the ICF representative
// of an identical empty `ret 4`, so this body was folded away; defined here, its class's unit, for the link.)
void EECPDiplomacyManager::Update(unsigned long inTime)
{
}
