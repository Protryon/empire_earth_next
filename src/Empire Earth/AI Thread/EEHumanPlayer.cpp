// C:\EEX\Empire Earth\AI Thread\EEHumanPlayer.cpp (file name inferred: no __FILE__; RTTI class EEHumanPlayer,
// vtable 0x43c330). The human player: 0xa543d..0xa5589 (EEHerd's code ends at 0xa543d, EEInfluenceTable's ctor starts
// at 0xa5589). The default ctor, then what its vtable brings: EEPlayer's four inline slots (EEHumanPlayer does not
// override them, EEComputerPlayer does: this is their first unit in link order), the inline GetName/SetName and the
// ??_G (the implicit dtor); then the second ctor and the Load/RepairPointers/Save wrappers.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"

// FUNCTION: EXE 0x4a543d
// Creates a human player with an empty name and its human-player pointer set to itself
EEHumanPlayer::EEHumanPlayer()
{
    this->mHumanName = "";
    this->mHumanPlayer = dynamic_cast<EEHumanPlayer*>(this);
}

// FUNCTION: EXE 0x4a5486
// Adds an amount to one of the player's resource caps
// (EEPlayer::AddResource, inline in ee/EEPlayer.h)

// FUNCTION: EXE 0x4a549a
// Returns a cost unchanged (no cost scaling for the player)
// (EEPlayer::ScaleCost, inline in ee/EEPlayer.h)

// FUNCTION: EXE 0x4a54a1
// Returns 1 for any unit class (extra population)
// (EEPlayer::GetExtraPopulation, inline in ee/EEPlayer.h)

// FUNCTION: EXE 0x4a54a7
// Sets the player's combat state
// (EEPlayer::SetCombatState, inline in ee/EEPlayer.h)

// FUNCTION: EXE 0x4a54b4
// Returns the human player's name
// (GetName, inline in ee/EEHumanPlayer.h)

// FUNCTION: EXE 0x4a54bb
// Sets the human player's name
// (SetName, inline in ee/EEHumanPlayer.h)

// SYNTHETIC: EXE 0x4a54ce
// (the scalar deleting destructor: the implicit dtor, ~EEPlayer out of line)

// FUNCTION: EXE 0x4a54fe
// Creates a named human player with a player id; its own name starts empty and its human-player pointer is itself
EEHumanPlayer::EEHumanPlayer(const UWideString& inName, unsigned long inPlayerID)
    : EEPlayer(inName, inPlayerID)
{
    this->mHumanName = "";
    this->mHumanPlayer = dynamic_cast<EEHumanPlayer*>(this);
}

// FUNCTION: EXE 0x4a5551
// Reads the player from a saved game (all of it is EEPlayer's); returns the read error
long EEHumanPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EEPlayer::Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x4a5565
// Restores the player's pointers after a load (all of it is EEPlayer's)
long EEHumanPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return EEPlayer::RepairPointers(inFirst, inSecond);
}

// FUNCTION: EXE 0x4a5575
// Writes the player to the saved game (all of it is EEPlayer's); returns the write error
long EEHumanPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EEPlayer::Save(inMajorVersion, inMinorVersion, outFile);
}
