// C:\EEX\Low-Level Engine\Utilities\UWideString.cpp
// Whole in Rust (rust/crates/ee_ustring wide.rs, LLE lane round 4; UWideString.h): what stays here touches C++ (the
// FSFile virtuals, the token vector, the checksum's entry and the varargs formatter).
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <mbstring.h>

#include "lle/LowLevelEngine.h"

// FUNCTION: LLE 0x10083b0a
// Formats into the string's buffer (grown first when the text does not fit: the original wrote past it), then
// updates its character and byte lengths
long sprintf(UWideString& outString, const char* inFormat, ...)
{
    va_list theArguments;
    va_start(theArguments, inFormat);
    va_list theCopy;
    va_copy(theCopy, theArguments);
    int theNeeded = vsnprintf(NULL, 0, inFormat, theCopy);
    va_end(theCopy);
    unsigned long theSize = theNeeded < 0 ? 0 : (unsigned long)theNeeded + 1;
    char* theBuffer = ee_uwstring_format_buffer(&outString.mState, theSize);
    long theResult = vsprintf(theBuffer, inFormat, theArguments);
    va_end(theArguments);
    ee_uwstring_format_end(&outString.mState, theBuffer, theSize);
    return theResult;
}

// FUNCTION: LLE 0x10083c38
UWideString::~UWideString()
{
    ee_uwstring_free(&this->mState);
}

// FUNCTION: LLE 0x1008422b
// Adds a checksum of the characters (weighted by position, times the length) named after the text
void UWideString::GetChecksum(ESChecksum& ioChecksum)
{
    UString theName;
    sprintf(theName, "UWideString (%s)", (const char*)*this);
    ioChecksum.AddChecksum(theName, ee_uwstring_checksum(&this->mState));
}

// FUNCTION: LLE 0x100845b5
// Reads a size and that many bytes from the file; on failure leaves the string empty and returns the error
long UWideString::Load(FSFile& inFile)
{
    unsigned long theSize = 0;
    unsigned long theRead;
    long theResult = inFile.Read(theSize);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(ee_uwstring_load_buffer(&this->mState, theSize), theSize, theRead);
        if (theResult == kErrorOK) {
            ee_uwstring_load_end(&this->mState, theSize);   // port (D6): an old Windows-1252 string converted
            if (theRead != theSize)
                theResult = kErrorFailed;
        }
    }
    if (theResult != kErrorOK)
        this->Clear();
    return theResult;
}

// FUNCTION: LLE 0x10084670
// Writes the byte size and the bytes, both counting the terminator, to the file; returns the first error
long UWideString::Save(FSFile& inFile) const
{
    unsigned long theSize = this->GetBytesUsedToSave() - 4;
    unsigned long theWritten;
    long theResult = inFile.Write(theSize);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Write(*this, theSize, theWritten);
    if (theResult == kErrorOK && theWritten < theSize)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100846cb
// Splits the string at any of the separator characters into outTokens (ee_uwstring_tokenize gives the tokens'
// first and last characters), skipping empty tokens; always true
bool UWideString::Tokenize(const UWideString& inSeparators, std::vector<UWideString>& outTokens) const
{
    std::vector<uint32_t> thePairs(2 * (this->GetLength() + 1));
    uint32_t theCount = ee_uwstring_tokenize(&this->mState, &inSeparators.mState, thePairs.data());
    outTokens.clear();
    for (uint32_t i = 0; i < theCount; i++)
        outTokens.push_back(this->GetIndexedSubstring(thePairs[2 * i], thePairs[2 * i + 1]));
    return true;
}
