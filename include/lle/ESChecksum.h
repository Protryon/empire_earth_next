// C:\EEX\Low-Level Engine\Error System\ESChecksum.h (file name inferred): named checksums of the world database,
// compared between network peers. ESChecksum.cpp 0x28ca..0x36c5. Requires <vector> and UString.
#ifndef LLE_ESCHECKSUM_H
#define LLE_ESCHECKSUM_H

#include <stdint.h>
#include <vector>

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
    bool Compare(const ESChecksumEntry& inOther, UString& outError) const;   // 0x29f3

    UString mDescription;                   // +0x00
    uint32_t mChecksum;                     // +0x10 (32 bits on every data model: saved and compared between peers)
};

class LLE_API ESChecksum {                  // vtable 0x10097520 (dtor only); sizeof 0x18
public:
    ESChecksum(bool inVerbose = true);      // 0x2a50; default proven by the closure ??_F (0x2977: push 1)
    virtual ~ESChecksum() {}                // 0x2921 (inline)
    // implicit copy ctor (0x292f) and operator= (0x2956)

    void AddChecksum(const UString& inDescription, unsigned long inChecksum);   // 0x2a78
    bool Compare(const ESChecksum& inOther, std::vector<UString>& outErrors) const;   // 0x2b1b
    bool GetIsVerboseMode() const;          // 0x2c3e
    unsigned long GetTotalChecksum() const; // 0x2c42
    void PrintReport() const;               // 0x2c57

protected:
    void AddChecksum(unsigned long inChecksum);   // 0x2ac9: added to the last entry (one anonymous entry when
                                                  // not verbose)

    std::vector<ESChecksumEntry> mEntries;  // +0x04
    bool mVerbose;                          // +0x14
};

#endif
