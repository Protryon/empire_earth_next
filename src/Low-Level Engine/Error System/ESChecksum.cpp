// C:\EEX\Low-Level Engine\Error System\ESChecksum.cpp (file name inferred: no __FILE__; RTTI .?AVESChecksum@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The checksum set is Rust's (rust/crates/ee_error checksum.rs, LLE lane round 2): these are its forwarders.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/ESChecksum.h"
#include "rust/ee_error.h"

// FUNCTION: LLE 0x10002a50
// Creates an empty checksum set, verbose (one described entry per checksum) or summed
ESChecksum::ESChecksum(bool inVerbose) : mRust(ee_error_checksum_new(inVerbose))
{
}

ESChecksum::ESChecksum(const ESChecksum& inOther) : mRust(ee_error_checksum_clone(inOther.mRust))
{
}

ESChecksum& ESChecksum::operator=(const ESChecksum& inOther)
{
    ee_error_checksum_assign(this->mRust, inOther.mRust);
    return *this;
}

ESChecksum::~ESChecksum()
{
    ee_error_checksum_free(this->mRust);
}

// FUNCTION: LLE 0x10002a78
void ESChecksum::AddChecksum(const UString& inDescription, unsigned long inChecksum)
{
    ee_error_checksum_add(this->mRust, inDescription, inChecksum);
}

// FUNCTION: LLE 0x10002ac9
void ESChecksum::AddChecksum(unsigned long inChecksum)
{
    ee_error_checksum_add_value(this->mRust, inChecksum);
}

// FUNCTION: LLE 0x10002b1b
bool ESChecksum::Compare(const ESChecksum& inOther, std::vector<UString>& outErrors) const
{
    EeErrorTexts* theTexts;
    bool theResult = ee_error_checksum_compare(this->mRust, inOther.mRust, &theTexts);
    for (size_t i = 0; i < ee_error_texts_count(theTexts); i++)
        outErrors.push_back(UString(ee_error_texts_get(theTexts, i)));
    ee_error_texts_free(theTexts);
    return theResult;
}

// FUNCTION: LLE 0x10002c3e
bool ESChecksum::GetIsVerboseMode() const
{
    return ee_error_checksum_verbose(this->mRust);
}

// FUNCTION: LLE 0x10002c42
unsigned long ESChecksum::GetTotalChecksum() const
{
    return ee_error_checksum_total(this->mRust);
}
