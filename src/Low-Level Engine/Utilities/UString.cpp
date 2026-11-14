// C:\EEX\Low-Level Engine\Utilities\UString.cpp
// Whole in Rust (rust/crates/ee_ustring, LLE lane round 4; UString.h): what stays here touches C++ (the FSFile
// virtuals, the token vectors, the varargs formatter).
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngine.h"

// FUNCTION: LLE 0x10082e38
// Formats into the string's buffer (grown first when the text does not fit: the original wrote past it), then
// updates its length; returns vsprintf's result
long sprintf(UString& outString, const char* inFormat, ...)
{
    va_list theArguments;
    va_start(theArguments, inFormat);
    va_list theCopy;
    va_copy(theCopy, theArguments);
    int theNeeded = vsnprintf(NULL, 0, inFormat, theCopy);
    va_end(theCopy);
    unsigned long theSize = theNeeded < 0 ? 0 : (unsigned long)theNeeded + 1;
    char* theBuffer = ee_ustring_format_buffer(&outString.mState, theSize);
    long theResult = vsprintf(theBuffer, inFormat, theArguments);
    va_end(theArguments);
    ee_ustring_format_end(&outString.mState, theBuffer, theSize);
    return theResult;
}

// FUNCTION: LLE 0x10082f22
UString::~UString()
{
    ee_ustring_free(&this->mState);
}

// FUNCTION: LLE 0x1008342d
// Reads a length and that many characters from the file; on failure leaves the string empty and returns the error
long UString::Load(FSFile& inFile)
{
    unsigned long theLength = 0;   // (passed on below even when its read fails)
    unsigned long theRead;
    long theResult = inFile.Read(theLength);
    if (theResult == kErrorOK && theLength != 0) {
        theResult = inFile.Read(ee_ustring_load_buffer(&this->mState, theLength), theLength, theRead);
        if (theResult == kErrorOK && theRead != theLength)
            theResult = kErrorFailed;
    }
    ee_ustring_load_end(&this->mState, theResult == kErrorOK && theLength != 0, theLength);
    return theResult;
}

// FUNCTION: LLE 0x100835f2
// Writes the length and the characters, both counting the terminator, to the file; returns the first error
long UString::Save(FSFile& inFile) const
{
    unsigned long theLength = this->GetLength() + 1;
    long theResult = inFile.Write(theLength);
    if (theResult != kErrorOK)
        return theResult;
    unsigned long theWritten;
    theResult = inFile.Write(*this, theLength, theWritten);
    if (theResult == kErrorOK && theWritten < theLength)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100834bd (NestedTokenize), 0x10083642 (Tokenize)
// Splits the string at separators into outTokens (ee_ustring_tokenize gives the tokens' first and last bytes)
bool UString::Tokens(const UString& inSeparators, const UString* inBrackets, std::vector<UString>& outTokens) const
{
    std::vector<uint32_t> thePairs(2 * (this->GetLength() + 1));
    uint32_t theCount = ee_ustring_tokenize(&this->mState, &inSeparators.mState,
                                            inBrackets ? &inBrackets->mState : NULL, thePairs.data());
    outTokens.clear();
    for (uint32_t i = 0; i < theCount; i++)
        outTokens.push_back(this->GetIndexedSubstring(thePairs[2 * i], thePairs[2 * i + 1]));
    return true;
}
