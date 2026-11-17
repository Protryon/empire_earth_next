// The original ESChecksum and ESChecksumEntry (include/lle/ESChecksum.h and src/Low-Level Engine/Error System/
// ESChecksum.cpp at 810a3eb2, before they moved to rust/crates/ee_base/src/error), unchanged but for the includes, for the
// golden driver es_checksum_golden.cpp (which supplies UString and its sprintf).
// C:\EEX\Low-Level Engine\Error System\ESChecksum.h (file name inferred): named checksums of the world database,
// compared between network peers. ESChecksum.cpp 0x28ca..0x36c5. Requires <vector> and UString.
#ifndef LLE_ESCHECKSUM_H
#define LLE_ESCHECKSUM_H


// Every member inline: emitted at the head of ESChecksum.cpp (0x28ca..0x2921) in this order; the accessors are
// ICF-folded into identical bodies elsewhere (GetDescription 0x1093c, GetChecksum 0x4012b, SetChecksum 0x4c3eb).
class LLE_API ESChecksumEntry {             // proven sizeof 0x14 (vector stride, idiv 0x14)
public:
    ESChecksumEntry(const ESChecksumEntry& inEntry)
        : mDescription(inEntry.mDescription), mChecksum(inEntry.mChecksum) {}
    ESChecksumEntry() : mChecksum(0) {}
    void SetDescription(const UString& inDescription) { this->mDescription = inDescription; }
    const UString& GetDescription() const { return this->mDescription; }
    unsigned long GetChecksum() const { return this->mChecksum; }
    void SetChecksum(unsigned long inChecksum) { this->mChecksum = inChecksum; }
    // implicit operator= (0x2901) and dtor (0x291c): an implicit dtor is what lets vector<ESChecksumEntry>'s
    // _Destroy call ~UString directly (a user-declared `~ESChecksumEntry() {}` turns it into ??_G(0), insert 0x2f8d)

private:
    friend class ESChecksum;
    bool Compare(const ESChecksumEntry& inOther, UString& outError) const;

    UString mDescription;                   // +0x00
    uint32_t mChecksum;                     // +0x10 (32 bits on every data model: saved and compared between peers)
};

class LLE_API ESChecksum {                  // vtable 0x10097520 (dtor only); sizeof 0x18
public:
    ESChecksum(bool inVerbose = true);      // 0x2a50; default proven by the closure ??_F (0x2977: push 1)
    virtual ~ESChecksum() {}
    // implicit copy ctor (0x292f) and operator= (0x2956)

    void AddChecksum(const UString& inDescription, unsigned long inChecksum);
    bool Compare(const ESChecksum& inOther, std::vector<UString>& outErrors) const;
    bool GetIsVerboseMode() const;
    unsigned long GetTotalChecksum() const;
    void PrintReport() const;

protected:
    void AddChecksum(unsigned long inChecksum);   // 0x2ac9: added to the last entry (one anonymous entry when
                                                  // not verbose)

    std::vector<ESChecksumEntry> mEntries;  // +0x04
    bool mVerbose;                          // +0x14
};

#endif

// C:\EEX\Low-Level Engine\Error System\ESChecksum.cpp (file name inferred: no __FILE__; RTTI .?AVESChecksum@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


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

// Creates an empty checksum set, verbose (one tagged entry per checksum) or summed
ESChecksum::ESChecksum(bool inVerbose)
{
    this->mVerbose = inVerbose;
}

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

// Adds a value to the last checksum entry, creating the first entry if there is none
void ESChecksum::AddChecksum(unsigned long inChecksum)
{
    ESChecksumEntry theEntry;
    if (this->mEntries.size() == 0)
        this->mEntries.push_back(theEntry);
    ESChecksumEntry& theLast = this->mEntries.back();
    theLast.mChecksum += inChecksum;
}

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
            sprintf(theError, "ESChecksum::Compare(): Checksum sizes don't match!  Source size is %zu and incoming "
                    "size is %zu.\n", this->mEntries.size(), inOther.mEntries.size());
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

// Returns whether the checksum set keeps one tagged entry per checksum
bool ESChecksum::GetIsVerboseMode() const
{
    return this->mVerbose;
}

// Returns the sum of every entry's checksum
unsigned long ESChecksum::GetTotalChecksum() const
{
    uint32_t theTotal = 0;   // 32-bit sum on every data model (the total is saved and compared between peers)
    for (std::vector<ESChecksumEntry>::const_iterator theEntry = this->mEntries.begin(); theEntry != this->mEntries.end();
         ++theEntry)
        theTotal += theEntry->GetChecksum();
    return theTotal;
}

// Walks the entries and prints nothing (the report body is compiled out of the retail build)
void ESChecksum::PrintReport() const
{
    for (std::vector<ESChecksumEntry>::const_iterator theEntry = this->mEntries.begin(); theEntry != this->mEntries.end();
         ++theEntry)
        ;
}
