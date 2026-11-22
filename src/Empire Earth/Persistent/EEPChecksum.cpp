// C:\EEX\Empire Earth\Persistent\EEPChecksum.cpp (file name inferred: no __FILE__; the class family and its keys
// "EEPChecksum-..."). A saved ESChecksum: 0x149c3c..0x14a009 (where the unit ends is not checked yet).
// Unit flags: the exe's module set. Data: the vtables 0x442254/0x442234; the keys' guard byte 0x531a28.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"

#include "ee/EEPChecksum.h"
#include "ee/EEPersistentTag.h"
#include "rust/base/error.h"
#include "rust/game/gamecpp.h"


// Creates an empty saved checksum chunk, verbose or not
EEPChecksum::EEPChecksum(bool inVerbose)
    : ESChecksum(inVerbose), FSPersistent(kPersistentChecksum)
{
}

// Creates an empty verbose saved checksum chunk (persistent factory)
FSPersistent* EEPChecksum::CreateDefault() const
{
    return new EEPChecksum(true);
}

// Adds the chunk's total checksum to a running checksum as "EEPChecksum"
void EEPChecksum::GetChecksum(ESChecksum& ioChecksum) const
{
    unsigned long theChecksum = this->GetTotalChecksum();
    ioChecksum.AddChecksum("EEPChecksum", theChecksum);
}

// Load and Save are ee_game gamecpp persist_parts.rs (game core lane, round 9); their C++ calls and members are
// EEPChecksumOp's cases below.

// Reads the verbose flag and the described checksum entries from a saved game; returns the first read error
long EEPChecksum::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_pc_load(static_cast<FSPersistent*>(this), &inFile);
}

// Writes the verbose flag and the described checksum entries to a save; returns the first write error
long EEPChecksum::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_pc_save(static_cast<FSPersistent*>(this), &outFile);
}

// ------------------------------------------------------------------------- the checksum's C++ (gamecpp persist_parts.rs)

int64_t EECore9OpD(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))

// The checksum's C++ calls and members (EEPCalamityHelper.cpp's EEPCalamityOp default; the end of lane C): one
// statement each
int64_t EEPChecksumOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PcRust: return P64(static_cast<EEPChecksum*>((FSPersistent*)p)->GetRust());
    case O::PcUStrLoad: return ((UString*)p)->Load(*(FSFile*)a[0]);
    case O::PcUStrText: return P64((const char*)*(UString*)p);
    case O::PcSaveText: return UString((const char*)a[0]).Save(*(FSFile*)p);
    default: return EECore9OpD(inOp, p, a, f);
    }
}
