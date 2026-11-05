// C:\EEX\Low-Level Engine\Error System\ESChecksum.cpp (file name inferred: no __FILE__; RTTI .?AVESChecksum@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/ESChecksum.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10002cb1
// TEMPLATE: LLE 0x10002f3d
// SYNTHETIC: LLE 0x1000297f
// TEMPLATE: LLE 0x10002c67
// TEMPLATE: LLE 0x10002f05
// TEMPLATE: LLE 0x1000326d
// TEMPLATE: LLE 0x1000352a
// TEMPLATE: LLE 0x1000358e
// TEMPLATE: LLE 0x100035c3
// TEMPLATE: LLE 0x100035ed
// SYNTHETIC: LLE 0x10003694

// FUNCTION: LLE 0x100029f3
// Compares two checksum entries by tag, then value; on a mismatch writes the reason to outError and returns false
bool ESChecksumEntry::Compare(const ESChecksumEntry& inOther, UString& outError) const
{
    bool theResult = false;
    if (inOther.mDescription != this->mDescription)
        sprintf(outError, "ESChecksumEntry::Compare failed, tags don't match: %s != %s\n",
                (const char*)inOther.mDescription, (const char*)this->mDescription);
    else if (inOther.mChecksum != this->mChecksum)
        sprintf(outError, "ESChecksumEntry::Compare failed, checksums don't match: %u != %u (%s)\n",
                inOther.mChecksum, this->mChecksum, (const char*)inOther.mDescription);
    else
        theResult = true;
    return theResult;
}

// FUNCTION: LLE 0x10002a50
// Creates an empty checksum set, verbose (one tagged entry per checksum) or summed
ESChecksum::ESChecksum(bool inVerbose)
{
    this->mVerbose = inVerbose;
}

// FUNCTION: LLE 0x10002a78
// Adds a tagged checksum entry in verbose mode, else adds the value to the running sum
void ESChecksum::AddChecksum(const UString& inDescription, unsigned long inChecksum)
{
    ESChecksumEntry theEntry;
    if (this->mVerbose) {
        theEntry.SetDescription(inDescription);
        theEntry.SetChecksum(inChecksum);
        this->mEntries.push_back(theEntry);
    } else
        this->AddChecksum(inChecksum);
}

// FUNCTION: LLE 0x10002ac9
// Adds a value to the last checksum entry, creating the first entry if there is none
void ESChecksum::AddChecksum(unsigned long inChecksum)
{
    ESChecksumEntry theEntry;
    if (this->mEntries.size() == 0)
        this->mEntries.push_back(theEntry);
    ESChecksumEntry& theLast = this->mEntries.back();
    theLast.mChecksum += inChecksum;
}

// FUNCTION: LLE 0x10002b1b
// Compares two checksum sets entry by entry, collecting one error text per mismatch; false if any differ
bool ESChecksum::Compare(const ESChecksum& inOther, std::vector<UString>& outErrors) const
{
    std::vector<ESChecksumEntry>::const_iterator theOther = inOther.mEntries.begin();
    std::vector<ESChecksumEntry>::const_iterator theEntry = this->mEntries.begin();
    UString theError;
    bool theResult = true;
    if (this->GetIsVerboseMode() == inOther.GetIsVerboseMode()) {
        if (this->mEntries.size() == inOther.mEntries.size()) {
            for (; theEntry != this->mEntries.end(); ++theEntry, ++theOther) {
                if (!theEntry->Compare(*theOther, theError)) {
                    outErrors.push_back(theError);
                    theResult = false;
                }
            }
        } else {
            sprintf(theError, "ESChecksum::Compare(): Checksum sizes don't match!  Source size is %d and incoming "
                    "size is %d.\n", this->mEntries.size(), inOther.mEntries.size());
            outErrors.push_back(theError);
            theResult = false;
        }
    } else {
        sprintf(theError, "ESChecksum::Compare(): One checksum is in verbose mode, the other isn't.  Compare "
                "failed.\n");
        outErrors.push_back(theError);
        theResult = false;
    }
    return theResult;
}

// FUNCTION: LLE 0x10002c3e
// Returns whether the checksum set keeps one tagged entry per checksum
bool ESChecksum::GetIsVerboseMode() const
{
    return this->mVerbose;
}

// FUNCTION: LLE 0x10002c42
// Returns the sum of every entry's checksum
unsigned long ESChecksum::GetTotalChecksum() const
{
    uint32_t theTotal = 0;   // 32-bit sum on every data model (the total is saved and compared between peers)
    for (std::vector<ESChecksumEntry>::const_iterator theEntry = this->mEntries.begin(); theEntry != this->mEntries.end();
         ++theEntry)
        theTotal += theEntry->GetChecksum();
    return theTotal;
}

// FUNCTION: LLE 0x10002c57
// Walks the entries and prints nothing (the report body is compiled out of the retail build)
void ESChecksum::PrintReport() const
{
    for (std::vector<ESChecksumEntry>::const_iterator theEntry = this->mEntries.begin(); theEntry != this->mEntries.end();
         ++theEntry)
        ;
}

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100028ca
// SYNTHETIC: LLE 0x100028e5
// SYNTHETIC: LLE 0x100028f5
// SYNTHETIC: LLE 0x10002901
// SYNTHETIC: LLE 0x1000291c
// SYNTHETIC: LLE 0x10002921
// SYNTHETIC: LLE 0x1000292f
// SYNTHETIC: LLE 0x10002956
// SYNTHETIC: LLE 0x10002977
// TEMPLATE: LLE 0x10002e19
// TEMPLATE: LLE 0x10002e45
// SYNTHETIC: LLE 0x10002e71
// TEMPLATE: LLE 0x10002ecb
// SYNTHETIC: LLE 0x10002f8d
// TEMPLATE: LLE 0x10003518
// TEMPLATE: LLE 0x10003563
// TEMPLATE: LLE 0x1000361f
// TEMPLATE: LLE 0x1000364b
// TEMPLATE: LLE 0x10003668
// SYNTHETIC: LLE 0x100036b3
