// C:\EEX\Empire Earth\Persistent\EEPChecksum.cpp (file name inferred: no __FILE__; the class family and its keys
// "EEPChecksum-..."). A saved ESChecksum: 0x149c3c..0x14a009 (where the unit ends is not checked yet).
// Unit flags: the exe's module set. Data: the vtables 0x442254/0x442234; the keys' guard byte 0x531a28.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"

#include "ee/EEPChecksum.h"


// Adds the chunk's total checksum to a running checksum as "EEPChecksum"
void EEPChecksum::GetChecksum(ESChecksum& ioChecksum) const
{
    unsigned long theChecksum = this->GetTotalChecksum();
    ioChecksum.AddChecksum("EEPChecksum", theChecksum);
}

// Load and Save are ee_game gamecpp persist_parts.rs (game core lane, round 9); their C++ calls and members are
// EEPChecksumOp's cases below.

// ------------------------------------------------------------------------- the checksum's C++ (gamecpp persist_parts.rs)


#define P64(x) ((int64_t)(intptr_t)(x))

