// C:\EEX\Low-Level Engine\Error System\ESChecksum.h (file name inferred): named checksums of the world database,
// saved with a game (EEPChecksum) and compared between network peers. The set is Rust's (rust/crates/ee_base/src/error
// checksum.rs, LLE lane round 2; a vector of ESChecksumEntry before): ESChecksum keeps its vtable (EEPChecksum
// derives from it) and owns a pointer to the Rust set; copies clone it. The members are inline forwarders (ESChecksum.cpp is gone). Requires UString.
#ifndef LLE_ESCHECKSUM_H
#define LLE_ESCHECKSUM_H

#include <stdint.h>
#include <vector>
#include "lle/UString.h"
#include "lle/UWideString.h"
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

#endif
