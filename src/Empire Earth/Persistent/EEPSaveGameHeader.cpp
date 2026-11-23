// C:\EEX\Empire Earth\Persistent\EEPSaveGameHeader.cpp (file name inferred: no __FILE__; the class family and its
// keys "EEPSaveGameHeader-..."). The header of a saved game: 0x14b7a5..0x14bd64. Unit flags: the exe's module set.
// Data: the vtable 0x4422a8; the five class static keys share the guard byte 0x531b58 (bits 1..0x10, `$S`).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"

#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "rust/game/gamecpp.h"


// Creates a save header with placeholder name and description, version 0.1 and three players
EEPSaveGameHeader::EEPSaveGameHeader()
{
    this->mName = "Load game name";
    this->mDescription = "Load game description";
    this->mMajorVersion = 0;
    this->mL4E4 = 0;
    this->mChecksum = 0;
    this->mMinorVersion = 1;
    this->mNumPlayers = 3;
}

// The header's Load and Save are ee_game gamecpp persist_parts.rs (game core lane, round 9); the methods forward and
// their C++ calls and members are EEPHeaderOp's cases below.

// Reads the header's name/version part, then its body; returns the first read error
long EEPSaveGameHeader::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_hdr_load(this, &inFile);
}

// Opens a save file, copies it into memory and reads its header from there; returns the first error
long EEPSaveGameHeader::LoadFromFile(FSFile& inFile)
{
    return ee_gamecpp_hdr_load_from_file(this, &inFile);
}

// Writes the header's name/version part, then its body; returns the first write error
long EEPSaveGameHeader::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_hdr_save(this, &outFile);
}

// Adds nothing to the checksum (empty: folded into a shared empty function)
void EEPSaveGameHeader::GetChecksum(ESChecksum& ioChecksum) const
{
}

// ------------------------------------------------------------------------- the header's C++ (gamecpp persist_parts.rs)


#define HDR ((EEPSaveGameHeader*)p)
#define AFILE (*(FSFile*)a[0])

