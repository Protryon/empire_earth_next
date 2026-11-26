// Low-Level Engine.dll API as seen by its importers (inferred header).
// Declarations are proven from the importers' mangled names (exact signatures); class layouts are not proven
// and only declared where an importer's codegen shows them. Split into real per-subsystem headers once the
// engine's own source layout is known (C:\EEX\Low-Level Engine\...).
//
// Include <windows.h> first: the original headers were seen through its A/W macros
// (TSScheduler::CreateEvent is exported as CreateEventA).
#ifndef LLE_LOWLEVELENGINE_H
#define LLE_LOWLEVELENGINE_H

#include "lle/LowLevelEngineCore.h"

// ---- Strings: the engine's per-class headers
#include "lle/UString.h"

#include "lle/UWideString.h"

// ---- Error system checksums (ESChecksum.cpp)
// C:\EEX\Low-Level Engine\Error System\ESChecksum.h (file name inferred): named checksums of the world database,
// saved with a game (EEPChecksum) and compared between network peers. The set is Rust's (rust/crates/ee_base/src/error
// checksum.rs, LLE lane round 2; a vector of ESChecksumEntry before): ESChecksum keeps its vtable (EEPChecksum
// derives from it) and owns a pointer to the Rust set; copies clone it. The members are inline forwarders (ESChecksum.cpp is gone). Requires UString.
// (ESChecksum.h in the original: folded here in LLE/UI round 2)
#include <stdint.h>
#include <vector>
#include "rust/base/error.h"

class LLE_API ESChecksum {
public:
    // an empty set, verbose (one described entry per checksum) or summed
    ESChecksum(bool inVerbose = true) : mRust(ee_error_checksum_new(inVerbose)) {}
    ESChecksum(const ESChecksum& inOther) : mRust(ee_error_checksum_clone(inOther.mRust)) {}
    ESChecksum& operator=(const ESChecksum& inOther)
    {
        ee_error_checksum_assign(this->mRust, inOther.mRust);
        return *this;
    }
    virtual ~ESChecksum() { ee_error_checksum_free(this->mRust); }

    // a described entry in a verbose set; else the value is added to the one sum
    void AddChecksum(const UString& inDescription, unsigned long inChecksum) { ee_error_checksum_add(this->mRust, inDescription, inChecksum); }
    // whether inOther matches entry by entry; one error text per mismatch into outErrors
    bool Compare(const ESChecksum& inOther, std::vector<UString>& outErrors) const
        { return ee_error_checksum_compare(this->mRust, inOther.mRust, PushText, &outErrors); }
    EeErrorChecksum* GetRust() const { return this->mRust; }
    bool GetIsVerboseMode() const { return ee_error_checksum_verbose(this->mRust); }
    unsigned long GetTotalChecksum() const { return ee_error_checksum_total(this->mRust); }  // the entries' 32-bit sum

    void AddChecksum(unsigned long inChecksum)   // added to the last entry (one anonymous entry when not verbose)
    {
        ee_error_checksum_add_value(this->mRust, inChecksum);
    }

    static void PushText(void* inErrors, const char* inText) { static_cast<std::vector<UString>*>(inErrors)->push_back(UString(inText)); }

    EeErrorChecksum* mRust;                 // owned
};

// "UWideString (<the text>)" and the string's checksum (ee_base error checksum.rs)
inline void UWideString::GetChecksum(ESChecksum& ioChecksum) { ee_error_checksum_add_wide(ioChecksum.GetRust(), &this->mState); }

// ---- Random
#include "lle/URandom.h"

// ---- Geometry engine (GE) basic types
// the engine's per-class headers (GE2DPoint.cpp block 0x1000c129: UMFastMath (its empty square root table went in
// LLE/UI round 2), GE2DPoint, GEVector, GE3DPoint, GE3DLine; then GE3DPlane, in GE3DLine.h)
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"

#include "lle/GERectangle.h"

#include "lle/GETransformation.h"

// ---- Threading / scheduling (TS): the critical section's header and the Thread System's (TSScheduler.h: the
// events, the read-write lock, the threads, the scheduler)
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"

// ---- Reference counting
#include "lle/UReferenceCounter.h"

// ---- File system (FS): the engine's per-class headers (vtables from RTTI, layouts from the engine's own code)
#include "lle/FSFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
LLE_API void FSInitialize(const UString& inPath);     // proven ?FSInitialize@@YAXABVUString@@@Z (exe import)

// ---- Sound engine (SE): the engine's per-class headers. The game sees SEMixer and SEConductor (SESample, which the
// plug-ins saw, is test-only now: tests/sound). Engine units include the SE headers they use themselves: the SE
// classes' static data members would otherwise shift every engine unit's `$E` numbering (one number per static
// member a unit sees, see TSScheduler.h).
#ifndef LOWLEVELENGINE_EXPORTS
#include "lle/SEMixer.h"
#endif
#include "lle/LLEForward.h"
LLE_API long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow);   // Sound Engine.cpp

// ---- Network engine (NE)

#endif
