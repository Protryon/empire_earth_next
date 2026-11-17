// C:\EEX\Low-Level Engine\Error System\ESChecksum.h (file name inferred): named checksums of the world database,
// saved with a game (EEPChecksum) and compared between network peers. The set is Rust's (rust/crates/ee_base/src/error
// checksum.rs, LLE lane round 2; a vector of ESChecksumEntry before): ESChecksum keeps its vtable (EEPChecksum
// derives from it) and owns a pointer to the Rust set; copies clone it. Requires UString.
#ifndef LLE_ESCHECKSUM_H
#define LLE_ESCHECKSUM_H

#include <stdint.h>
#include <vector>

struct EeErrorChecksum;

class LLE_API ESChecksum {
public:
    ESChecksum(bool inVerbose = true);
    ESChecksum(const ESChecksum& inOther);
    ESChecksum& operator=(const ESChecksum& inOther);
    virtual ~ESChecksum();

    // a described entry in a verbose set; else the value is added to the one sum
    void AddChecksum(const UString& inDescription, unsigned long inChecksum);
    // whether inOther matches entry by entry; one error text per mismatch into outErrors
    bool Compare(const ESChecksum& inOther, std::vector<UString>& outErrors) const;
    bool GetIsVerboseMode() const;
    unsigned long GetTotalChecksum() const; // the entries' 32-bit sum

protected:
    void AddChecksum(unsigned long inChecksum);   // added to the last entry (one anonymous entry when not verbose)

    EeErrorChecksum* mRust;                 // owned
};

#endif
