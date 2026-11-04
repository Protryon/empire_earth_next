// C:\EEX\Low-Level Engine\Utilities\UWideString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <mbstring.h>

#include "lle/LowLevelEngine.h"

#define MB(s) ((unsigned char*)(s))

// Compiler-generated for this unit (names in analysis/lle/names.txt):

unsigned long UWideString::sDefaultAllocationLength = 256;

// FUNCTION: LLE 0x100838f7
// Returns a new multibyte string holding the left string followed by the right one
UWideString operator+(const UWideString& inLeft, const UWideString& inRight)
{
    UWideString theResult(inLeft.mLength + inRight.mLength + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// FUNCTION: LLE 0x10083945
// Returns a new multibyte string holding the string followed by the C string
UWideString operator+(const UWideString& inLeft, const char* inRight)
{
    UWideString theResult(inLeft.GetLength() + strlen(inRight) + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// FUNCTION: LLE 0x10083998
// Returns whether the two strings are equal (multibyte compare)
bool operator==(const UWideString& inLeft, const UWideString& inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB((const char*)inRight)) == 0;
}

// FUNCTION: LLE 0x100839b6
// Returns whether the string equals the C string (multibyte compare)
bool operator==(const UWideString& inLeft, const char* inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB(inRight)) == 0;
}

// FUNCTION: LLE 0x100839d0
// Returns whether the two strings differ (multibyte compare)
bool operator!=(const UWideString& inLeft, const UWideString& inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB((const char*)inRight)) != 0;
}

// FUNCTION: LLE 0x100839ef
// Returns whether the string differs from the C string (multibyte compare)
bool operator!=(const UWideString& inLeft, const char* inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB(inRight)) != 0;
}

// FUNCTION: LLE 0x10083a0a
// Returns whether the left string sorts before the right one (multibyte compare)
bool operator<(const UWideString& inLeft, const UWideString& inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB((const char*)inRight)) < 0;
}

// FUNCTION: LLE 0x10083a2c
// Returns whether the string sorts before the C string (multibyte compare)
bool operator<(const UWideString& inLeft, const char* inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB(inRight)) < 0;
}

// FUNCTION: LLE 0x10083a4a
// Returns whether the left string sorts before or equal to the right one (multibyte compare)
bool operator<=(const UWideString& inLeft, const UWideString& inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB((const char*)inRight)) <= 0;
}

// FUNCTION: LLE 0x10083a6c
// Returns whether the string sorts before or equal to the C string (multibyte compare)
bool operator<=(const UWideString& inLeft, const char* inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB(inRight)) <= 0;
}

// FUNCTION: LLE 0x10083a8a
// Returns whether the left string sorts after the right one (multibyte compare)
bool operator>(const UWideString& inLeft, const UWideString& inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB((const char*)inRight)) > 0;
}

// FUNCTION: LLE 0x10083aac
// Returns whether the string sorts after the C string (multibyte compare)
bool operator>(const UWideString& inLeft, const char* inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB(inRight)) > 0;
}

// FUNCTION: LLE 0x10083aca
// Returns whether the left string sorts after or equal to the right one (multibyte compare)
bool operator>=(const UWideString& inLeft, const UWideString& inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB((const char*)inRight)) >= 0;
}

// FUNCTION: LLE 0x10083aec
// Returns whether the string sorts after or equal to the C string (multibyte compare)
bool operator>=(const UWideString& inLeft, const char* inRight)
{
    return _mbscmp(MB((const char*)inLeft), MB(inRight)) >= 0;
}

// FUNCTION: LLE 0x10083b0a
// Formats into the string's buffer with no capacity check, then updates its character and byte lengths
long sprintf(UWideString& outString, const char* inFormat, ...)
{
    va_list theArguments;
    va_start(theArguments, inFormat);
    long theResult = vsprintf(outString.mString, inFormat, theArguments);
    outString.mLength = _mbslen(MB(outString.mString));
    outString.mByteLength = _mbsnbcnt(MB((const char*)outString), outString.mLength);
    return theResult;
}

// FUNCTION: LLE 0x10083b46
// Creates an empty multibyte string with the default capacity (256 bytes)
UWideString::UWideString()
{
    this->mCapacity = UWideString::sDefaultAllocationLength;
    this->mString = new char[this->mCapacity];
    this->mLength = 0;
    this->mByteLength = 0;
    strcpy(this->mString, "");
}

// FUNCTION: LLE 0x10083b7b
// Creates a copy of a multibyte string with the same capacity
UWideString::UWideString(const UWideString& inString)
{
    this->mLength = inString.mLength;
    this->mByteLength = inString.mByteLength;
    this->mCapacity = inString.mCapacity;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString.mString);
}

// FUNCTION: LLE 0x10083bb7
// Creates a multibyte string from a C string, with room for two bytes per character
UWideString::UWideString(const char* inString)
{
    this->mLength = _mbslen(MB(inString));
    this->mByteLength = _mbsnbcnt(MB(inString), this->mLength);
    this->mCapacity = this->mLength * 2 + 2;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString);
}

// FUNCTION: LLE 0x10083bfe
// Creates an empty multibyte string with room for inLength characters of two bytes each
UWideString::UWideString(unsigned long inLength)
{
    this->mCapacity = inLength * 2 + 2;
    this->mString = new char[this->mCapacity];
    this->mLength = 0;
    this->mByteLength = 0;
    strcpy(this->mString, "");
}

// FUNCTION: LLE 0x10083c38
// Destroys the multibyte string, freeing its buffer
UWideString::~UWideString()
{
    delete[] this->mString;
}

// FUNCTION: LLE 0x10083c48
// Copies another string, growing to its capacity or shrinking to whole 256-byte blocks when much smaller
UWideString& UWideString::operator=(const UWideString& inString)
{
    if (inString.mLength == 0) {
        this->mString[0] = 0;
        this->mLength = 0;
        this->mByteLength = 0;
    } else {
        if (inString.mCapacity != this->mCapacity) {
            if (inString.mCapacity > this->mCapacity) {
                delete[] this->mString;
                this->mCapacity = inString.mCapacity;
                this->mString = new char[this->mCapacity];
            } else {
                unsigned long theBlocks = (inString.mCapacity - 1) / UWideString::sDefaultAllocationLength + 1;
                if ((long)((this->mCapacity - 1) / UWideString::sDefaultAllocationLength - theBlocks + 1) > 0) {
                    delete[] this->mString;
                    this->mCapacity = UWideString::sDefaultAllocationLength * theBlocks;
                    this->mString = new char[this->mCapacity];
                }
            }
        }
        if (inString.mString)
            strcpy(this->mString, inString.mString);
        this->mLength = _mbslen(MB((const char*)*this));
        this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
    }
    return *this;
}

// FUNCTION: LLE 0x10083cf5
// Copies a C string, growing to fit it or shrinking to whole 256-byte blocks when much smaller
UWideString& UWideString::operator=(const char* inString)
{
    unsigned long theSize = _mbsnbcnt(MB(inString), _mbslen(MB(inString))) + 2;
    if (theSize != this->mCapacity) {
        if (theSize > this->mCapacity) {
            delete[] this->mString;
            this->mCapacity = theSize;
            this->mString = new char[theSize];
        } else {
            unsigned long theBlocks = (theSize - 1) / UWideString::sDefaultAllocationLength + 1;
            if ((long)((this->mCapacity - 1) / UWideString::sDefaultAllocationLength - theBlocks + 1) > 0) {
                delete[] this->mString;
                this->mCapacity = UWideString::sDefaultAllocationLength * theBlocks;
                this->mString = new char[this->mCapacity];
            }
        }
    }
    strcpy(this->mString, inString);
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
    return *this;
}

// FUNCTION: LLE 0x10083d99
// Copies the first inCount characters of a string, resizing the buffer as operator= does
void UWideString::CopyString(const UWideString& inString, unsigned long inCount)
{
    if (inString.mLength == 0) {
        this->mString[0] = 0;
        this->mLength = 0;
        this->mByteLength = 0;
        return;
    }
    if (inString.mCapacity != this->mCapacity) {
        if (inString.mCapacity > this->mCapacity) {
            delete[] this->mString;
            this->mCapacity = inString.mCapacity;
            this->mString = new char[this->mCapacity];
        } else {
            unsigned long theBlocks = (inString.mCapacity - 1) / UWideString::sDefaultAllocationLength + 1;
            if ((long)((this->mCapacity - 1) / UWideString::sDefaultAllocationLength - theBlocks + 1) > 0) {
                delete[] this->mString;
                this->mCapacity = UWideString::sDefaultAllocationLength * theBlocks;
                this->mString = new char[this->mCapacity];
            }
        }
    }
    _mbsncpy(MB((const char*)*this), MB(inString.mString), inCount);
    char* theEnd = (char*)_mbsninc(MB((const char*)*this), inCount);
    strcpy(theEnd, "");
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
}

// FUNCTION: LLE 0x10083e59
// Copies the first inCount characters of a C string, resizing the buffer to fit them
void UWideString::CopyString(const char* inString, unsigned long inCount)
{
    unsigned long theSize = _mbsnbcnt(MB(inString), inCount) + 1;
    if (theSize != this->mCapacity) {
        if (theSize > this->mCapacity) {
            delete[] this->mString;
            this->mCapacity = theSize;
            this->mString = new char[theSize];
        } else {
            unsigned long theBlocks = (theSize - 1) / UWideString::sDefaultAllocationLength + 1;
            if ((long)((this->mCapacity - 1) / UWideString::sDefaultAllocationLength - theBlocks + 1) > 0) {
                delete[] this->mString;
                this->mCapacity = UWideString::sDefaultAllocationLength * theBlocks;
                this->mString = new char[this->mCapacity];
            }
        }
    }
    _mbsncpy(MB((const char*)*this), MB(inString), inCount);
    char* theEnd = (char*)_mbsninc(MB((const char*)*this), inCount);
    strcpy(theEnd, "");
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
}

// FUNCTION: LLE 0x10083f0f
// Returns the first byte of the character at an index, or 0 when the index is out of range
char UWideString::operator[](int inIndex) const
{
    if (inIndex >= 0 && inIndex < (int)this->mLength)
        return *_mbsninc(MB((const char*)*this), inIndex);
    return 0;
}

// FUNCTION: LLE 0x10083f32
// Appends a string, reallocating to the combined size when it does not fit (tested against free space + 2)
UWideString& UWideString::operator+=(const UWideString& inString)
{
    unsigned long theLength = _mbslen(MB((const char*)*this));
    char* theString = this->mString;
    unsigned long theAddedLength = _mbslen(MB(inString.mString));
    unsigned long theAddedSize = _mbsnbcnt(MB((const char*)inString), theAddedLength);
    unsigned long theSize = _mbsnbcnt(MB(theString), theLength) + theAddedSize + 2;
    if (theSize < this->mCapacity - _mbsnbcnt(MB((const char*)*this), _mbslen(MB((const char*)*this))) + 2)
        strcat(this->mString, inString.mString);
    else {
        this->mCapacity = theSize;
        char* theNewString = new char[theSize];
        strcpy(theNewString, this->mString);
        strcat(theNewString, inString.mString);
        delete this->mString;
        this->mString = theNewString;
    }
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
    return *this;
}

// FUNCTION: LLE 0x10084003
// Appends a C string, reallocating to the combined size when it does not fit (tested against free space + 2)
UWideString& UWideString::operator+=(const char* inString)
{
    unsigned long theLength = _mbslen(MB((const char*)*this));
    char* theString = this->mString;
    unsigned long theAddedSize = _mbsnbcnt(MB(inString), _mbslen(MB(inString)));
    unsigned long theSize = _mbsnbcnt(MB(theString), theLength) + theAddedSize + 2;
    if (theSize < this->mCapacity - _mbsnbcnt(MB((const char*)*this), _mbslen(MB((const char*)*this))) + 2)
        strcat(this->mString, inString);
    else {
        this->mCapacity = theSize;
        char* theNewString = new char[theSize];
        strcpy(theNewString, this->mString);
        strcat(theNewString, inString);
        delete this->mString;
        this->mString = theNewString;
    }
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
    return *this;
}

// FUNCTION: LLE 0x100840be
// Writes a 4-byte size, the bytes and a 2-byte zero terminator to outData
void UWideString::Export(unsigned char* outData) const
{
    unsigned long theLength = strlen(this->mString);
    unsigned long theTerminator = 0;
    unsigned long theSize = theLength + 2;
    memcpy(outData, &theSize, 4);
    memcpy(outData + 4, this->mString, theLength);
    memcpy(outData + theLength + 4, &theTerminator, 2);
}

// FUNCTION: LLE 0x10084112
// Returns the first character position at or after inStart where a string occurs, or the length when not found
unsigned long UWideString::Find(const UWideString& inString, unsigned long inStart) const
{
    unsigned long theIndex = inStart;
    unsigned long theLength = _mbslen(MB(inString.mString));
    long theLast = (long)(this->mLength - inString.mLength) > 0 ? this->mLength - inString.mLength : 0;
    unsigned long theResult = this->mLength;
    if (this->mLength >= theLength) {
        unsigned char* thePosition = _mbsninc(MB((const char*)*this), inStart);
        for (; theIndex <= (unsigned long)theLast; theIndex++, thePosition = _mbsinc(thePosition)) {
            if (_mbsncmp(thePosition, MB((const char*)inString), inString.GetLength()) == 0) {
                theResult = theIndex;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10084197
// Returns the first character position at or after inStart where a C string occurs, or the length if not found
unsigned long UWideString::Find(const char* inString, unsigned long inStart) const
{
    unsigned long theIndex = inStart;
    unsigned long theLength = _mbslen(MB(inString));
    long theLast = (long)(this->mLength - theLength) > 0 ? this->mLength - theLength : 0;
    unsigned long theResult = this->mLength;
    if (this->mLength >= theLength) {
        unsigned char* thePosition = _mbsninc(MB((const char*)*this), inStart);
        for (; theIndex <= (unsigned long)theLast; theIndex++, thePosition = _mbsinc(thePosition)) {
            if (_mbsncmp(thePosition, MB(inString), theLength) == 0) {
                theResult = theIndex;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10084217
// Returns the bytes Save writes: a 4-byte size plus the bytes and terminator
unsigned long UWideString::GetBytesUsedToSave() const
{
    return _mbsnbcnt(MB((const char*)*this), this->GetLength()) + 5;
}

// FUNCTION: LLE 0x1008422b
// Adds a checksum of the characters (weighted by position, times the length) named after the text
void UWideString::GetChecksum(ESChecksum& ioChecksum)
{
    unsigned long theSum = 0;
    UString theName;
    for (unsigned long i = 0; i < this->mLength; i++)
        theSum += this->mString[i] * (i + 2);
    unsigned long theChecksum = this->mLength * theSum;
    sprintf(theName, "UWideString (%s)", this->mString);
    ioChecksum.AddChecksum(theName, theChecksum);
}

// FUNCTION: LLE 0x10084296
// Returns the bytes Export writes: a 4-byte size, the bytes and a 2-byte terminator
unsigned long UWideString::GetExportSize() const
{
    return strlen(this->mString) + 6;
}

// FUNCTION: LLE 0x100842a3
// Returns the characters from inFirst through inLast (inclusive) as a new string
UWideString UWideString::GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
{
    UWideString theResult;
    theResult = (const char*)_mbsninc(MB((const char*)*this), inFirst);
    char* theEnd = (char*)_mbsninc(MB(theResult.mString), inLast - inFirst + 1);
    strcpy(theEnd, "");
    return theResult;
}

// FUNCTION: LLE 0x10084305
// Reads a size-prefixed string from inData, growing the buffer if needed; returns the bytes consumed
unsigned long UWideString::Import(unsigned char* inData)
{
    unsigned long theSize;
    unsigned char* theString = inData + 4;
    unsigned long theLength = _mbslen(theString);
    memcpy(&theSize, inData, 4);
    if (this->mCapacity < theSize)
        this->Reallocate(theSize);
    memcpy(this->mString, theString, theSize);
    this->mLength = theLength;
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), theLength);
    return theSize + 4;
}

// FUNCTION: LLE 0x1008436b
// Inserts a string at a character position (inReplace drops the old tail); when it does not fit, the new
// buffer gets the needed size
void UWideString::InsertString(const UWideString& inString, unsigned long inPosition, bool inReplace)
{
    unsigned long theSize;
    if (inReplace)
        theSize = _mbsnbcnt(MB((const char*)*this), inPosition) +
                  _mbsnbcnt(MB(inString.mString), _mbslen(MB(inString.mString))) + 2;
    else {
        unsigned long theLength = _mbslen(MB((const char*)*this));
        theSize = _mbsnbcnt(MB((const char*)*this), theLength) +
                  _mbsnbcnt(MB(inString.mString), _mbslen(MB(inString.mString))) + 2;
    }
    unsigned long theAvailable = this->mCapacity - _mbsnbcnt(MB((const char*)*this), _mbslen(MB((const char*)*this))) + 2;
    // Fixed: the capacity used to be set to the free space (theAvailable), not the needed size, so the copies
    // below overflowed the new buffer
    if (theSize > theAvailable || theSize > this->mCapacity)
        this->mCapacity = theSize;
    char* theNewString = new char[this->mCapacity];
    _mbsncpy(MB(theNewString), MB((const char*)*this), inPosition);
    char* theInsert = (char*)_mbsninc(MB(theNewString), inPosition);
    strcpy(theInsert, inString.mString);
    char* theRest = (char*)_mbsninc(MB((const char*)*this), inPosition);
    if (!inReplace)
        strcat(theInsert, theRest);
    delete[] this->mString;
    this->mString = theNewString;
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
}

// FUNCTION: LLE 0x10084498
// Inserts a C string at a character position (inReplace drops the old tail); when it does not fit, the new
// buffer gets the needed size
void UWideString::InsertString(const char* inString, unsigned long inPosition, bool inReplace)
{
    unsigned long theSize;
    if (inReplace)
        theSize = _mbsnbcnt(MB((const char*)*this), inPosition) + _mbsnbcnt(MB(inString), _mbslen(MB(inString))) + 2;
    else {
        unsigned long theLength = _mbslen(MB((const char*)*this));
        theSize = _mbsnbcnt(MB((const char*)*this), theLength) + _mbsnbcnt(MB(inString), _mbslen(MB(inString))) + 2;
    }
    unsigned long theAvailable = this->mCapacity - _mbsnbcnt(MB((const char*)*this), _mbslen(MB((const char*)*this))) + 2;
    // Fixed: the capacity used to be set to the free space (theAvailable), not the needed size, so the copies
    // below overflowed the new buffer
    if (theSize > theAvailable || theSize > this->mCapacity)
        this->mCapacity = theSize;
    char* theNewString = new char[this->mCapacity];
    _mbsncpy(MB(theNewString), MB((const char*)*this), inPosition);
    char* theInsert = (char*)_mbsninc(MB(theNewString), inPosition);
    strcpy(theInsert, inString);
    char* theRest = (char*)_mbsninc(MB((const char*)*this), inPosition);
    if (!inReplace)
        strcat(theInsert, theRest);
    delete[] this->mString;
    this->mString = theNewString;
    this->mLength = _mbslen(MB((const char*)*this));
    this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
}

// FUNCTION: LLE 0x100845b5
// Reads a size and that many bytes from the file; on failure leaves the string empty and returns the error
long UWideString::Load(FSFile& inFile)
{
    unsigned long theSize;
    unsigned long theRead;
    long theResult = inFile.Read(theSize);
    if (theResult == kErrorOK) {
        if (this->mCapacity < theSize + 1) {
            this->mCapacity = theSize + 1;
            delete[] this->mString;
            this->mString = new char[this->mCapacity];
        }
        memset(this->mString, 0, theSize + 1);
        theResult = inFile.Read(this->mString, theSize, theRead);
        if (theResult == kErrorOK) {
            strcpy(this->mString + theSize, "");
            this->mLength = _mbslen(MB((const char*)*this));
            this->mByteLength = _mbsnbcnt(MB((const char*)*this), this->mLength);
            if (theRead != theSize)
                theResult = kErrorFailed;
        }
    }
    if (theResult != kErrorOK) {
        this->mString[0] = 0;
        this->mLength = 0;
        this->mByteLength = 0;
    }
    return theResult;
}

// FUNCTION: LLE 0x10084670
// Writes the byte size and the bytes, both counting the terminator, to the file; returns the first error
long UWideString::Save(FSFile& inFile) const
{
    unsigned long theSize = _mbsnbcnt(MB((const char*)*this), this->GetLength()) + 1;
    unsigned long theWritten;
    long theResult = inFile.Write(theSize);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Write(this->mString, theSize, theWritten);
    if (theResult == kErrorOK && theWritten < theSize)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100846cb
// Splits the string at any of the separator characters into outTokens, skipping empty tokens; always true
bool UWideString::Tokenize(const UWideString& inSeparators, std::vector<UWideString>& outTokens) const
{
    unsigned long theNotFound = inSeparators.mLength;
    unsigned long theStart = 0;
    bool theBetweenTokens = true;
    char theCharacter[3];
    outTokens.clear();
    memset(theCharacter, 0, 3);
    unsigned long i = 0;
    unsigned char* thePosition = MB((const char*)*this);
    for (; i <= this->mLength; i++) {
        if (theBetweenTokens) {
            _mbsncpy(MB(theCharacter), thePosition, 1);
            char* theEnd = (char*)_mbsninc(MB(theCharacter), 1);
            strcpy(theEnd, "");
            if (inSeparators.Find(theCharacter, 0) == theNotFound) {
                theBetweenTokens = false;
                theStart = i;
            }
        } else {
            _mbsncpy(MB(theCharacter), thePosition, 1);
            char* theEnd = (char*)_mbsninc(MB(theCharacter), 1);
            strcpy(theEnd, "");
            if (inSeparators.Find(theCharacter, 0) != theNotFound || i == this->mLength) {
                outTokens.push_back(this->GetIndexedSubstring(theStart, i - 1));
                theBetweenTokens = true;
            }
        }
        thePosition = _mbsinc(thePosition);
    }
    return true;
}
