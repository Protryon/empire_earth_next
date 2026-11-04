// C:\EEX\Low-Level Engine\Utilities\UString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngine.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x100836e3
// TEMPLATE: LLE 0x1008371e
// TEMPLATE: LLE 0x10083789

unsigned long UString::sDefaultAllocationLength = 256;

// FUNCTION: LLE 0x10082c43
// Returns a new string holding the left string followed by the right one
UString operator+(const UString& inLeft, const UString& inRight)
{
    UString theResult(inLeft.GetLength() + inRight.GetLength() + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// FUNCTION: LLE 0x10082c91
// Returns a new string holding the string followed by the C string
UString operator+(const UString& inLeft, const char* inRight)
{
    UString theResult(inLeft.GetLength() + strlen(inRight) + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// FUNCTION: LLE 0x10082ce4
// Returns whether the two strings are equal (case-sensitive)
bool operator==(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) == 0;
}

// FUNCTION: LLE 0x10082cff
// Returns whether the string equals the C string (case-sensitive)
bool operator==(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) == 0;
}

// FUNCTION: LLE 0x10082d17
// Returns whether the two strings differ (case-sensitive)
bool operator!=(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) != 0;
}

// FUNCTION: LLE 0x10082d33
// Returns whether the string differs from the C string (case-sensitive)
bool operator!=(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) != 0;
}

// FUNCTION: LLE 0x10082d4c
// Returns whether the left string sorts before the right one (strcmp order)
bool operator<(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) < 0;
}

// FUNCTION: LLE 0x10082d6b
// Returns whether the string sorts before the C string (strcmp order)
bool operator<(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) < 0;
}

// FUNCTION: LLE 0x10082d87
// Returns whether the left string sorts before or equal to the right one (strcmp order)
bool operator<=(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) <= 0;
}

// FUNCTION: LLE 0x10082da6
// Returns whether the string sorts before or equal to the C string (strcmp order)
bool operator<=(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) <= 0;
}

// FUNCTION: LLE 0x10082dc2
// Returns whether the left string sorts after the right one (strcmp order)
bool operator>(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) > 0;
}

// FUNCTION: LLE 0x10082de1
// Returns whether the string sorts after the C string (strcmp order)
bool operator>(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) > 0;
}

// FUNCTION: LLE 0x10082dfd
// Returns whether the left string sorts after or equal to the right one (strcmp order)
bool operator>=(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) >= 0;
}

// FUNCTION: LLE 0x10082e1c
// Returns whether the string sorts after or equal to the C string (strcmp order)
bool operator>=(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) >= 0;
}

// FUNCTION: LLE 0x10082e38
// Formats into the string's buffer with no capacity check, then updates its length; returns vsprintf's result
long sprintf(UString& outString, const char* inFormat, ...)
{
    va_list theArguments;
    va_start(theArguments, inFormat);
    long theResult = vsprintf(outString.mString, inFormat, theArguments);
    outString.mLength = strlen(outString.mString);
    return theResult;
}

// FUNCTION: LLE 0x10082e65
// Creates an empty string with the default capacity (256 bytes)
UString::UString()
{
    this->mCapacity = UString::sDefaultAllocationLength;
    this->mString = new char[UString::sDefaultAllocationLength];
    this->mLength = 0;
    this->mString[0] = 0;
}

// FUNCTION: LLE 0x10082e90
// Creates a copy of a string with the same capacity
UString::UString(const UString& inString)
{
    this->mLength = inString.mLength;
    this->mCapacity = inString.mCapacity;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString.mString);
}

// FUNCTION: LLE 0x10082ec6
// Creates a string from a C string, sized exactly to fit it
UString::UString(const char* inString)
{
    this->mLength = strlen(inString);
    this->mCapacity = this->mLength + 1;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString);
}

// FUNCTION: LLE 0x10082efb
// Creates an empty string with the given capacity in bytes
UString::UString(unsigned long inCapacity)
{
    this->mCapacity = inCapacity;
    this->mString = new char[inCapacity];
    this->mLength = 0;
    this->mString[0] = 0;
}

// FUNCTION: LLE 0x10082f22
// Destroys the string, freeing its buffer
UString::~UString()
{
    delete[] this->mString;
}

// FUNCTION: LLE 0x10082f32
// Copies another string, growing the buffer to the source's capacity if that is larger
UString& UString::operator=(const UString& inString)
{
    unsigned long theSize = inString.mCapacity;
    if (theSize > this->mCapacity) {
        delete[] this->mString;
        this->mString = new char[inString.mCapacity];
        this->mCapacity = inString.mCapacity;
        theSize = inString.mCapacity;
    }
    if (inString.mString)
        memcpy(this->mString, inString.mString, theSize);
    if (theSize > 0) {
        this->mString[theSize - 1] = 0;
        this->mLength = strlen(this->mString);
    }
    return *this;
}

// FUNCTION: LLE 0x10082f94
// Copies a C string, reallocating to fit when it does not fit the current capacity
UString& UString::operator=(const char* inString)
{
    unsigned long theSize;
    if (strlen(inString) < this->mCapacity - 1)
        theSize = strlen(inString) + 1;
    else {
        delete[] this->mString;
        this->mString = new char[strlen(inString) + 1];
        theSize = this->mCapacity = strlen(inString) + 1;
    }
    memcpy(this->mString, inString, theSize);
    if (theSize > 0) {
        this->mString[theSize - 1] = 0;
        this->mLength = strlen(this->mString);
    }
    return *this;
}

// FUNCTION: LLE 0x1008300c
// Returns the character at an index, or 0 when the index is out of range
char UString::operator[](int inIndex) const
{
    if (inIndex >= 0 && inIndex < (int)this->mLength)
        return this->mString[inIndex];
    return 0;
}

// FUNCTION: LLE 0x10083026
// Appends a string, truncated to the space left in the buffer (never grows)
UString& UString::operator+=(const UString& inString)
{
    unsigned long theSpace = this->mCapacity - this->mLength - 1;
    if (theSpace) {
        unsigned long theCount = inString.mLength;
        if (theCount >= theSpace)
            theCount = theSpace;
        memcpy(this->mString + this->mLength, inString.mString, theCount);
        this->mString[this->mLength + theCount] = 0;
        this->mLength = strlen(this->mString);
    }
    return *this;
}

// FUNCTION: LLE 0x10083070
// Appends a C string, truncated to the space left in the buffer (never grows)
UString& UString::operator+=(const char* inString)
{
    unsigned long theCount = this->mCapacity - this->mLength - 1;
    if (theCount) {
        unsigned long theLength = strlen(inString);
        if (theLength < theCount)
            theCount = theLength;
        memcpy(this->mString + this->mLength, inString, theCount);
        (this->mString + this->mLength)[theCount] = 0;
        this->mLength = strlen(this->mString);
    }
    return *this;
}

// FUNCTION: LLE 0x100830bd
// Shrinks the buffer to exactly the string's length plus the terminator
void UString::Compact()
{
    char* theString = new char[this->mLength + 1];
    this->mCapacity = this->mLength + 1;
    memcpy(theString, this->mString, this->mLength);
    theString[this->mLength] = 0;
    delete[] this->mString;
    this->mString = theString;
}

// FUNCTION: LLE 0x100830f8
// Copies inCount characters from inStart of a string to inDestination, optionally reallocating first
UString& UString::Copy(const UString& inString, unsigned long inStart, unsigned long inCount,
                       unsigned long inDestination, bool inResize)
{
    if (inStart < inString.mCapacity) {
        if (inResize && this->mCapacity != inCount) {
            delete[] this->mString;
            this->mCapacity = inCount + 1;
            this->mString = new char[this->mCapacity];
            this->mLength = 0;
            this->mString[0] = 0;
        }
        if (inStart + inCount >= inString.mCapacity)
            inCount = inString.mCapacity - inStart;
        if (inDestination < this->mCapacity) {
            unsigned long theCount = this->mCapacity - inDestination - 1;
            if (theCount > inCount)
                theCount = inCount;
            memcpy(this->mString + inDestination, inString.mString + inStart, theCount);
            (this->mString + theCount)[inDestination] = 0;
            this->mLength = strlen(this->mString);
        }
    }
    return *this;
}

// FUNCTION: LLE 0x1008319d
// Copies inCount characters from inStart of a C string to inDestination, optionally reallocating first
UString& UString::Copy(const char* inString, unsigned long inStart, unsigned long inCount,
                       unsigned long inDestination, bool inResize)
{
    unsigned long theLength = strlen(inString);
    if (inStart < theLength) {
        if (inResize && this->mCapacity != inCount) {
            delete[] this->mString;
            this->mCapacity = inCount + 1;
            this->mString = new char[this->mCapacity];
            this->mLength = 0;
            this->mString[0] = 0;
        }
        if (inStart + inCount > theLength)
            inCount = theLength - inStart;
        if (inDestination < this->mCapacity) {
            unsigned long theCount = this->mCapacity - inDestination;
            if (theCount > inCount)
                theCount = inCount;
            // Fixed: a copy that filled the buffer wrote its terminator one byte past the allocation; the buffer
            // now grows by that byte, keeping the copied characters
            if (inDestination + theCount >= this->mCapacity) {
                char* theString = new char[inDestination + theCount + 1];
                memcpy(theString, this->mString, this->mCapacity);
                delete[] this->mString;
                this->mString = theString;
                this->mCapacity = inDestination + theCount + 1;
            }
            memcpy(this->mString + inDestination, inString + inStart, theCount);
            (this->mString + theCount)[inDestination] = 0;
            this->mLength = strlen(this->mString);
        }
    }
    return *this;
}

// FUNCTION: LLE 0x1008323e
// Returns the first position at or after inStart where a string occurs, or the length when not found
unsigned long UString::Find(const UString& inString, unsigned long inStart) const
{
    unsigned long theIndex = this->mLength;
    if (inString.mLength <= this->mLength - inStart)
        for (unsigned long i = inStart; i <= this->mLength - inString.mLength; i++)
            if (strncmp(this->mString + i, inString.mString, inString.mLength) == 0) {
                theIndex = i;
                break;
            }
    return theIndex;
}

// FUNCTION: LLE 0x10083297
// Returns the first position at or after inStart where a C string occurs, or the length when not found
unsigned long UString::Find(const char* inString, unsigned long inStart) const
{
    unsigned long theLength = strlen(inString);
    unsigned long theIndex = this->mLength;
    if (theLength <= this->mLength - inStart)
        for (unsigned long i = inStart; i <= this->mLength - theLength; i++)
            if (strncmp(this->mString + i, inString, theLength) == 0) {
                theIndex = i;
                break;
            }
    return theIndex;
}

// FUNCTION: LLE 0x100832f6
// Returns the last position at or before inStart where a string starts, or the length when not found
unsigned long UString::FindBackwards(const UString& inString, unsigned long inStart) const
{
    unsigned long theIndex = this->mLength;
    if (inStart > this->mLength)
        inStart = this->mLength;
    for (long i = inStart - inString.mLength; i >= 0; i--)
        if (strncmp(this->mString + i, inString.mString, inString.mLength) == 0) {
            theIndex = i;
            break;
        }
    return theIndex;
}

// FUNCTION: LLE 0x10083343
// Returns the last position at or before inStart where a C string starts, or the length when not found
unsigned long UString::FindBackwards(const char* inString, unsigned long inStart) const
{
    unsigned long theLength = strlen(inString);
    unsigned long theIndex = this->mLength;
    if (inStart > this->mLength)
        inStart = this->mLength;
    for (long i = inStart - theLength; i >= 0; i--)
        if (strncmp(this->mString + i, inString, theLength) == 0) {
            theIndex = i;
            break;
        }
    return theIndex;
}

// FUNCTION: LLE 0x10083394
// Returns the bytes Save writes: a 4-byte length plus the characters and terminator
unsigned long UString::GetBytesUsedToSave() const
{
    return this->mLength + 5;
}

// FUNCTION: LLE 0x1008339b
// Returns inLength characters starting at inStart as a new string
UString UString::GetSubstring(unsigned long inStart, unsigned long inLength) const
{
    UString theResult(inLength + 1);
    theResult.Copy(*this, inStart, inLength, 0, false);
    return theResult;
}

// FUNCTION: LLE 0x100833e2
// Returns the characters from inFirst through inLast (inclusive) as a new string
UString UString::GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
{
    unsigned long theLength = inLast - inFirst;
    UString theResult(theLength + 2);
    theResult.Copy(*this, inFirst, theLength + 1, 0, false);
    return theResult;
}

// FUNCTION: LLE 0x1008342d
// Reads a length and that many characters from the file; on failure leaves the string empty and returns the error
long UString::Load(FSFile& inFile)
{
    unsigned long theLength;
    unsigned long theRead;
    long theResult = inFile.Read(theLength);
    if (theResult == kErrorOK && theLength != 0) {
        this->mCapacity = theLength + 1;
        delete[] this->mString;
        this->mString = new char[this->mCapacity];
        theResult = inFile.Read(this->mString, theLength, theRead);
        if (theResult == kErrorOK && theRead == theLength) {
            this->mString[theLength] = 0;
            this->mLength = strlen(this->mString);
        } else {
            if (theResult == kErrorOK)
                theResult = kErrorFailed;
            if (this->mString)
                this->mString[0] = 0;
            this->mLength = 0;
        }
    } else {
        if (this->mString)
            this->mString[0] = 0;
        this->mLength = 0;
    }
    return theResult;
}

// FUNCTION: LLE 0x100834bd
// Splits the string at separators into outTokens, keeping bracketed parts whole and stripping outer brackets
bool UString::NestedTokenize(const UString& inSeparators, const UString& inBrackets,
                             std::vector<UString>& outTokens) const
{
    bool theQuoted = false;
    unsigned long theNotFound = inSeparators.mLength;
    unsigned long i = 0;
    unsigned long theDepth = 0;
    unsigned long theStart = 0;
    bool theBetweenTokens = true;
    char theCharacter[2];
    outTokens.clear();
    theCharacter[1] = 0;
    for (; i < this->mLength + 1; i++) {
        if (theBetweenTokens) {
            theCharacter[0] = this->mString[i];
            if (inSeparators.Find(theCharacter, 0) == theNotFound) {
                theBetweenTokens = false;
                theStart = i;
                theQuoted = theCharacter[0] == inBrackets.mString[0];
                if (theQuoted)
                    theDepth = 1;
            }
        } else {
            theCharacter[0] = this->mString[i];
            if (theCharacter[0] == inBrackets.mString[0] &&
                (inBrackets.mString[1] != inBrackets.mString[0] || theDepth == 0))
                theDepth++;
            else if (theCharacter[0] == inBrackets.mString[1] && theDepth > 0)
                theDepth--;
            if (theDepth == 0 && (inSeparators.Find(theCharacter, 0) != theNotFound || i == this->mLength)) {
                unsigned long theEnd = i - 1;
                if (theQuoted && this->mString[theEnd] == inBrackets.mString[1]) {
                    theStart++;
                    theEnd--;
                }
                if (theStart <= theEnd)
                    outTokens.push_back(this->GetIndexedSubstring(theStart, theEnd));
                theQuoted = false;
                theBetweenTokens = true;
            }
        }
    }
    return true;
}

// FUNCTION: LLE 0x100835f2
// Writes the length and the characters, both counting the terminator, to the file; returns the first error
long UString::Save(FSFile& inFile) const
{
    unsigned long theLength = this->mLength + 1;
    long theResult = inFile.Write(theLength);
    if (theResult != kErrorOK)
        return theResult;
    unsigned long theWritten;
    theResult = inFile.Write(this->mString, this->mLength + 1, theWritten);
    if (theResult == kErrorOK && theWritten < this->mLength + 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10083642
// Splits the string at any of the separator characters into outTokens, skipping empty tokens; always true
// The character load is written in each branch (VC6 hoists the common head; the [base+index] order of the merged
// load is the tell: one load before the if gave [edi+eax]).
bool UString::Tokenize(const UString& inSeparators, std::vector<UString>& outTokens) const
{
    unsigned long theNotFound = inSeparators.mLength;
    unsigned long i = 0;
    unsigned long theStart = 0;
    bool theBetweenTokens = true;
    char theCharacter[2];
    outTokens.clear();
    theCharacter[1] = 0;
    for (; i < this->mLength + 1; i++) {
        if (theBetweenTokens) {
            theCharacter[0] = this->mString[i];
            if (inSeparators.Find(theCharacter, 0) == theNotFound) {
                theBetweenTokens = false;
                theStart = i;
            }
        } else {
            theCharacter[0] = this->mString[i];
            if (inSeparators.Find(theCharacter, 0) != theNotFound || i == this->mLength) {
                    outTokens.push_back(this->GetIndexedSubstring(theStart, i - 1));
                theBetweenTokens = true;
            }
        }
    }
    return true;
}
