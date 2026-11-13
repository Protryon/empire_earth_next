// C:\EEX\Empire Earth\Persistent\EEPChecksum.cpp (file name inferred: no __FILE__; the class family and its keys
// "EEPChecksum-..."). A saved ESChecksum: 0x149c3c..0x14a009 (where the unit ends is not checked yet).
// Unit flags: the exe's module set. Data: the vtables 0x442254/0x442234; the keys' guard byte 0x531a28.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"

#include "ee/EEPChecksum.h"
#include "ee/EEPersistentTag.h"
#include "rust/ee_error.h"

const UString EEPChecksum::sNumChecksums("EEPChecksum-NumChecksums");
const UString EEPChecksum::sChecksum("EEPChecksum-Checksum");
const UString EEPChecksum::sVerboseMode("EEPChecksum-VerboseMode");

// FUNCTION: EXE 0x549d05
// Creates an empty saved checksum chunk, verbose or not
EEPChecksum::EEPChecksum(bool inVerbose)
    : ESChecksum(inVerbose), FSPersistent(kPersistentChecksum)
{
}

// FUNCTION: EXE 0x549d3c
// Creates an empty verbose saved checksum chunk (persistent factory)
FSPersistent* EEPChecksum::CreateDefault() const
{
    return new EEPChecksum(true);
}

// FUNCTION: EXE 0x549d5f
// Adds the chunk's total checksum to a running checksum as "EEPChecksum"
void EEPChecksum::GetChecksum(ESChecksum& ioChecksum) const
{
    unsigned long theChecksum = this->GetTotalChecksum();
    ioChecksum.AddChecksum("EEPChecksum", theChecksum);
}

// FUNCTION: EXE 0x549d9b
// Reads the verbose flag and the described checksum entries from a saved game; returns the first read error
long EEPChecksum::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theCount = 0;
    UString theDescription;
    unsigned long theValue;
    bool theVerbose;
    ee_error_checksum_clear(this->mRust);
    long theResult = inFile.Read(theVerbose);
    if (theResult != kErrorOK)
        return theResult;
    ee_error_checksum_set_verbose(this->mRust, theVerbose);
    theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (long i = 0; i < theCount && theResult == kErrorOK; i++) {
        theResult = theDescription.Load(inFile);
        if (theResult != kErrorOK)
            continue;
        theResult = inFile.Read(theValue);
        if (theResult == kErrorOK)
            ee_error_checksum_push(this->mRust, theDescription, (uint32_t)theValue);
    }
    return theResult;
}

// FUNCTION: EXE 0x549e5c
// Writes the verbose flag and the described checksum entries to a save; returns the first write error
long EEPChecksum::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    size_t theCount = ee_error_checksum_count(this->mRust);
    long theResult = outFile.Write(ee_error_checksum_verbose(this->mRust));
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write((long)theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (size_t i = 0; i < theCount && theResult == kErrorOK; i++) {
        uint32_t theValue;
        std::vector<char> theText(ee_error_checksum_entry(this->mRust, i, NULL, 0, &theValue) + 1);
        ee_error_checksum_entry(this->mRust, i, theText.data(), theText.size(), &theValue);
        theResult = UString(theText.data()).Save(outFile);
        if (theResult == kErrorOK)
            theResult = outFile.Write((unsigned long)theValue);
    }
    return theResult;
}

// FUNCTION: EXE 0x549fdc
// Destroys the saved checksum chunk
// the inline destructor (ee/EEPChecksum.h), after its first user

// SYNTHETIC: EXE 0x549c3c
// SYNTHETIC: EXE 0x549c46
// SYNTHETIC: EXE 0x549c57
// SYNTHETIC: EXE 0x549c63
// SYNTHETIC: EXE 0x549c7f
// SYNTHETIC: EXE 0x549c89
// SYNTHETIC: EXE 0x549c9a
// SYNTHETIC: EXE 0x549ca6
// SYNTHETIC: EXE 0x549cc2
// SYNTHETIC: EXE 0x549ccc
// SYNTHETIC: EXE 0x549cdd
// SYNTHETIC: EXE 0x549ce9
