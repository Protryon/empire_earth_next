// The original UWideString (include/lle/UWideString.h and src/Low-Level Engine/Utilities/UWideString.cpp at
// f37096a0, before its storage moved to rust/crates/ee_ustring), unchanged but for the includes, for the golden
// driver uws_golden.cpp (which supplies FSFile, ESChecksum, UString, the platform's string functions and the
// error codes).
// C:\EEX\Low-Level Engine\Utilities\UWideString.h (RECONSTRUCTION; UWideString.cpp 0x838f7..0x847e1). A multibyte
// (MBCS) string: lengths through _mbslen/_mbsnbcnt; UTF-8 since decision D6 (platform/PlatString.h). Capacity grows in blocks of sDefaultAllocationLength.
#ifndef LLE_UWIDESTRING_H
#define LLE_UWIDESTRING_H


class FSFile;
class ESChecksum;

class LLE_API UWideString {                // proven sizeof 0x14 (TCP NPIMGetStatus stack local)
public:
    UWideString();
    UWideString(const UWideString& inString);
    UWideString(const char* inString);
    UWideString(unsigned long inLength);    // capacity for inLength characters (2 * inLength + 2 bytes)
    virtual ~UWideString();
    UWideString& operator=(const UWideString& inString);
    UWideString& operator=(const char* inString);
    // exported ??BUWideString@@QBEPBDXZ = `mov eax,[ecx+4]; ret` (ICF-folded with UString's); TCP 0x2539 strcpy
    operator const char*() const { return this->mString; }
    void CopyString(const UWideString& inString, unsigned long inCount);
    void CopyString(const char* inString, unsigned long inCount);
    char operator[](int inIndex) const;
    UWideString& operator+=(const UWideString& inString);
    UWideString& operator+=(const char* inString);
    void Export(unsigned char* outData) const;
    unsigned long Find(const UWideString& inString, unsigned long inStart) const;
    unsigned long Find(const char* inString, unsigned long inStart) const;
    unsigned long GetBytesUsedToSave() const;
    void GetChecksum(ESChecksum& ioChecksum);
    unsigned long GetExportSize() const;
    UWideString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const;
    unsigned long Import(unsigned char* inData);
    void InsertString(const UWideString& inString, unsigned long inPosition, bool inReplace);
    void InsertString(const char* inString, unsigned long inPosition, bool inReplace);
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    bool Tokenize(const UWideString& inSeparators, std::vector<UWideString>& outTokens) const;
    // proven inline (exported COMDATs emitted by other units: 0x1000..0x1140 in Error System.cpp's range, the getters
    // ICF-folded at 0x4495 / 0x44c71 / 0x4012b)
    unsigned long GetLength() const { return this->mLength; }
    unsigned long GetTCHARLength() const { return this->mByteLength; }
    unsigned long GetAllocatedLength() const { return this->mCapacity; }
    void Clear() { this->mString[0] = 0; this->mLength = 0; this->mByteLength = 0; }
    bool Equals(const UWideString& inString) const
        { return PlatMbsCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString)) == 0; }
    bool Equals(const UWideString& inString, unsigned long inCount) const
        { return PlatMbsNBCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString),
                           inCount) == 0; }
    bool Equals(const char* inString) const
        { return PlatMbsCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString)) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const
        { return PlatMbsNBCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString), inCount) == 0; }
    bool EqualsIgnoreCase(const UWideString& inString) const
        { return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString)) == 0; }
    bool EqualsIgnoreCase(const UWideString& inString, unsigned long inCount) const
        { return PlatMbsNBICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString),
                            inCount) == 0; }
    bool EqualsIgnoreCase(const char* inString) const
        { return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString)) == 0; }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const
        { return PlatMbsNBICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString), inCount) == 0; }
    void ToLower() { PlatMbsLwr(reinterpret_cast<unsigned char*>(this->mString)); }
    void ToUpper() { PlatMbsUpr(reinterpret_cast<unsigned char*>(this->mString)); }
    // Replaces the buffer with an empty one of inSize + 1 bytes. (The original dropped the fresh buffer and pointed
    // mString at a "" literal, which Import then overwrote and the destructor freed.)
    void Reallocate(unsigned long inSize)
    {
        delete[] this->mString;
        this->mLength = 0;
        this->mByteLength = 0;
        this->mCapacity = inSize + 1;
        this->mString = new char[this->mCapacity];
        this->mString[0] = '\0';
    }
    // after Reallocate: the first unit emits it last (0x10001136)
#ifdef LOWLEVELENGINE_EXPORTS
    static void SetDefaultAllocationLength(unsigned long inLength) { UWideString::sDefaultAllocationLength = inLength; }
#else
    static void SetDefaultAllocationLength(unsigned long inLength);
#endif
private:
    friend LLE_API UWideString operator+(const UWideString& inLeft, const UWideString& inRight);
    friend LLE_API UWideString operator+(const UWideString& inLeft, const char* inRight);
    friend LLE_API long sprintf(UWideString& outString, const char* inFormat, ...);   // sets the lengths

    char* mString;                          // +0x04
    unsigned long mByteLength;              // +0x08 bytes (_mbsnbcnt)
    unsigned long mLength;                  // +0x0c characters (_mbslen)
    unsigned long mCapacity;                // +0x10 allocated bytes

    // Inferred engine-only visibility: see TSScheduler.h ($E numbering of the plug-ins)
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long sDefaultAllocationLength;   // exported ?sDefaultAllocationLength@UWideString@@0KA (0x10da58)
#endif
};

LLE_API UWideString operator+(const UWideString& inLeft, const UWideString& inRight);
LLE_API UWideString operator+(const UWideString& inLeft, const char* inRight);
LLE_API bool operator==(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator==(const UWideString& inLeft, const char* inRight);
LLE_API bool operator!=(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator!=(const UWideString& inLeft, const char* inRight);
LLE_API bool operator<(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator<(const UWideString& inLeft, const char* inRight);
LLE_API bool operator<=(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator<=(const UWideString& inLeft, const char* inRight);
LLE_API bool operator>(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator>(const UWideString& inLeft, const char* inRight);
LLE_API bool operator>=(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator>=(const UWideString& inLeft, const char* inRight);
LLE_API long sprintf(UWideString& outString, const char* inFormat, ...) PLAT_PRINTF_FORMAT(2, 3);

#endif
// C:\EEX\Low-Level Engine\Utilities\UWideString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


#define MB(s) ((unsigned char*)(s))

// Compiler-generated for this unit (names in analysis/lle/names.txt):

unsigned long UWideString::sDefaultAllocationLength = 256;

// Returns a new multibyte string holding the left string followed by the right one
UWideString operator+(const UWideString& inLeft, const UWideString& inRight)
{
    UWideString theResult(inLeft.mLength + inRight.mLength + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// Returns a new multibyte string holding the string followed by the C string
UWideString operator+(const UWideString& inLeft, const char* inRight)
{
    UWideString theResult(inLeft.GetLength() + strlen(inRight) + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// Returns whether the two strings are equal (multibyte compare)
bool operator==(const UWideString& inLeft, const UWideString& inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB((const char*)inRight)) == 0;
}

// Returns whether the string equals the C string (multibyte compare)
bool operator==(const UWideString& inLeft, const char* inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB(inRight)) == 0;
}

// Returns whether the two strings differ (multibyte compare)
bool operator!=(const UWideString& inLeft, const UWideString& inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB((const char*)inRight)) != 0;
}

// Returns whether the string differs from the C string (multibyte compare)
bool operator!=(const UWideString& inLeft, const char* inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB(inRight)) != 0;
}

// Returns whether the left string sorts before the right one (multibyte compare)
bool operator<(const UWideString& inLeft, const UWideString& inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB((const char*)inRight)) < 0;
}

// Returns whether the string sorts before the C string (multibyte compare)
bool operator<(const UWideString& inLeft, const char* inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB(inRight)) < 0;
}

// Returns whether the left string sorts before or equal to the right one (multibyte compare)
bool operator<=(const UWideString& inLeft, const UWideString& inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB((const char*)inRight)) <= 0;
}

// Returns whether the string sorts before or equal to the C string (multibyte compare)
bool operator<=(const UWideString& inLeft, const char* inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB(inRight)) <= 0;
}

// Returns whether the left string sorts after the right one (multibyte compare)
bool operator>(const UWideString& inLeft, const UWideString& inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB((const char*)inRight)) > 0;
}

// Returns whether the string sorts after the C string (multibyte compare)
bool operator>(const UWideString& inLeft, const char* inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB(inRight)) > 0;
}

// Returns whether the left string sorts after or equal to the right one (multibyte compare)
bool operator>=(const UWideString& inLeft, const UWideString& inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB((const char*)inRight)) >= 0;
}

// Returns whether the string sorts after or equal to the C string (multibyte compare)
bool operator>=(const UWideString& inLeft, const char* inRight)
{
    return PlatMbsCmp(MB((const char*)inLeft), MB(inRight)) >= 0;
}

// Formats into the string's buffer with no capacity check, then updates its character and byte lengths
long sprintf(UWideString& outString, const char* inFormat, ...)
{
    va_list theArguments;
    va_start(theArguments, inFormat);
    long theResult = vsprintf(outString.mString, inFormat, theArguments);
    outString.mLength = PlatMbsLen(MB(outString.mString));
    outString.mByteLength = PlatMbsNBCnt(MB((const char*)outString), outString.mLength);
    return theResult;
}

// Creates an empty multibyte string with the default capacity (256 bytes)
UWideString::UWideString()
{
    this->mCapacity = UWideString::sDefaultAllocationLength;
    this->mString = new char[this->mCapacity];
    this->mLength = 0;
    this->mByteLength = 0;
    strcpy(this->mString, "");
}

// Creates a copy of a multibyte string with the same capacity
UWideString::UWideString(const UWideString& inString)
{
    this->mLength = inString.mLength;
    this->mByteLength = inString.mByteLength;
    this->mCapacity = inString.mCapacity;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString.mString);
}

// Creates a multibyte string from a C string, with room for two bytes per character
UWideString::UWideString(const char* inString)
{
    this->mLength = PlatMbsLen(MB(inString));
    this->mByteLength = PlatMbsNBCnt(MB(inString), this->mLength);
    this->mCapacity = this->mLength * 2 + 2;
    // port (decision D6): a UTF-8 character takes up to 4 bytes, so two a character can be too few (two euro signs
    // are 7 bytes with the terminator, the room 6: strcpy overran the buffer): the room is at least the bytes and the
    // terminator (text of 1- and 2-byte characters keeps the original's capacity)
    if (this->mCapacity < this->mByteLength + 1)
        this->mCapacity = this->mByteLength + 1;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString);
}

// Creates an empty multibyte string with room for inLength characters of two bytes each
UWideString::UWideString(unsigned long inLength)
{
    this->mCapacity = inLength * 2 + 2;
    this->mString = new char[this->mCapacity];
    this->mLength = 0;
    this->mByteLength = 0;
    strcpy(this->mString, "");
}

// Destroys the multibyte string, freeing its buffer
UWideString::~UWideString()
{
    delete[] this->mString;
}

// Copies another string, growing to its capacity or shrinking to whole 256-byte blocks when much smaller
UWideString& UWideString::operator=(const UWideString& inString)
{
    // divergence: self-assignment returns at once (the original strcpy'd the buffer onto itself, UB that ASan stops on:
    // the post-game summary assigns a status bar's GEText to itself in AddStatusBar)
    if (this == &inString)
        return *this;
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
        this->mLength = PlatMbsLen(MB((const char*)*this));
        this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
    }
    return *this;
}

// Copies a C string, growing to fit it or shrinking to whole 256-byte blocks when much smaller
UWideString& UWideString::operator=(const char* inString)
{
    unsigned long theSize = PlatMbsNBCnt(MB(inString), PlatMbsLen(MB(inString))) + 2;
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
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
    return *this;
}

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
    PlatMbsNCpy(MB((const char*)*this), MB(inString.mString), inCount);
    char* theEnd = (char*)PlatMbsNInc(MB((const char*)*this), inCount);
    strcpy(theEnd, "");
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
}

// Copies the first inCount characters of a C string, resizing the buffer to fit them
void UWideString::CopyString(const char* inString, unsigned long inCount)
{
    unsigned long theSize = PlatMbsNBCnt(MB(inString), inCount) + 1;
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
    PlatMbsNCpy(MB((const char*)*this), MB(inString), inCount);
    char* theEnd = (char*)PlatMbsNInc(MB((const char*)*this), inCount);
    strcpy(theEnd, "");
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
}

// Returns the first byte of the character at an index, or 0 when the index is out of range
char UWideString::operator[](int inIndex) const
{
    if (inIndex >= 0 && inIndex < (int)this->mLength)
        return *PlatMbsNInc(MB((const char*)*this), inIndex);
    return 0;
}

// Appends a string, reallocating to the combined size when it does not fit (tested against free space + 2)
UWideString& UWideString::operator+=(const UWideString& inString)
{
    unsigned long theLength = PlatMbsLen(MB((const char*)*this));
    char* theString = this->mString;
    unsigned long theAddedLength = PlatMbsLen(MB(inString.mString));
    unsigned long theAddedSize = PlatMbsNBCnt(MB((const char*)inString), theAddedLength);
    unsigned long theSize = PlatMbsNBCnt(MB(theString), theLength) + theAddedSize + 2;
    if (theSize < this->mCapacity - PlatMbsNBCnt(MB((const char*)*this), PlatMbsLen(MB((const char*)*this))) + 2)
        strcat(this->mString, inString.mString);
    else {
        this->mCapacity = theSize;
        char* theNewString = new char[theSize];
        strcpy(theNewString, this->mString);
        strcat(theNewString, inString.mString);
        delete this->mString;
        this->mString = theNewString;
    }
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
    return *this;
}

// Appends a C string, reallocating to the combined size when it does not fit (tested against free space + 2)
UWideString& UWideString::operator+=(const char* inString)
{
    unsigned long theLength = PlatMbsLen(MB((const char*)*this));
    char* theString = this->mString;
    unsigned long theAddedSize = PlatMbsNBCnt(MB(inString), PlatMbsLen(MB(inString)));
    unsigned long theSize = PlatMbsNBCnt(MB(theString), theLength) + theAddedSize + 2;
    if (theSize < this->mCapacity - PlatMbsNBCnt(MB((const char*)*this), PlatMbsLen(MB((const char*)*this))) + 2)
        strcat(this->mString, inString);
    else {
        this->mCapacity = theSize;
        char* theNewString = new char[theSize];
        strcpy(theNewString, this->mString);
        strcat(theNewString, inString);
        delete this->mString;
        this->mString = theNewString;
    }
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
    return *this;
}

// Writes a 4-byte size, the bytes and a 2-byte zero terminator to outData
void UWideString::Export(unsigned char* outData) const
{
    unsigned long theLength = strlen(this->mString);
    uint16_t theTerminator = 0;
    uint32_t theSize = theLength + 2;       // 4 bytes on the wire on every data model
    memcpy(outData, &theSize, 4);
    memcpy(outData + 4, this->mString, theLength);
    memcpy(outData + theLength + 4, &theTerminator, 2);
}

// Returns the first character position at or after inStart where a string occurs, or the length when not found
unsigned long UWideString::Find(const UWideString& inString, unsigned long inStart) const
{
    unsigned long theIndex = inStart;
    unsigned long theLength = PlatMbsLen(MB(inString.mString));
    long theLast = (long)(this->mLength - inString.mLength) > 0 ? this->mLength - inString.mLength : 0;
    unsigned long theResult = this->mLength;
    if (this->mLength >= theLength) {
        unsigned char* thePosition = PlatMbsNInc(MB((const char*)*this), inStart);
        for (; theIndex <= (unsigned long)theLast; theIndex++, thePosition = PlatMbsInc(thePosition)) {
            if (PlatMbsNCmp(thePosition, MB((const char*)inString), inString.GetLength()) == 0) {
                theResult = theIndex;
                break;
            }
        }
    }
    return theResult;
}

// Returns the first character position at or after inStart where a C string occurs, or the length if not found
unsigned long UWideString::Find(const char* inString, unsigned long inStart) const
{
    unsigned long theIndex = inStart;
    unsigned long theLength = PlatMbsLen(MB(inString));
    long theLast = (long)(this->mLength - theLength) > 0 ? this->mLength - theLength : 0;
    unsigned long theResult = this->mLength;
    if (this->mLength >= theLength) {
        unsigned char* thePosition = PlatMbsNInc(MB((const char*)*this), inStart);
        for (; theIndex <= (unsigned long)theLast; theIndex++, thePosition = PlatMbsInc(thePosition)) {
            if (PlatMbsNCmp(thePosition, MB(inString), theLength) == 0) {
                theResult = theIndex;
                break;
            }
        }
    }
    return theResult;
}

// Returns the bytes Save writes: a 4-byte size plus the bytes and terminator
unsigned long UWideString::GetBytesUsedToSave() const
{
    return PlatMbsNBCnt(MB((const char*)*this), this->GetLength()) + 5;
}

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

// Returns the bytes Export writes: a 4-byte size, the bytes and a 2-byte terminator
unsigned long UWideString::GetExportSize() const
{
    return strlen(this->mString) + 6;
}

// Returns the characters from inFirst through inLast (inclusive) as a new string
UWideString UWideString::GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
{
    UWideString theResult;
    theResult = (const char*)PlatMbsNInc(MB((const char*)*this), inFirst);
    char* theEnd = (char*)PlatMbsNInc(MB(theResult.mString), inLast - inFirst + 1);
    strcpy(theEnd, "");
    return theResult;
}

// Reads a size-prefixed string from inData, growing the buffer if needed; returns the bytes consumed
unsigned long UWideString::Import(unsigned char* inData)
{
    uint32_t theSize;                       // 4 bytes on the wire (unsigned long is 8 on LP64)
    unsigned char* theString = inData + 4;
    unsigned long theLength = PlatMbsLen(theString);
    memcpy(&theSize, inData, 4);
    if (this->mCapacity < theSize)
        this->Reallocate(theSize);
    memcpy(this->mString, theString, theSize);
    this->mLength = theLength;
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), theLength);
    return theSize + 4;
}

// Inserts a string at a character position (inReplace drops the old tail); when it does not fit, the new
// buffer gets the needed size
void UWideString::InsertString(const UWideString& inString, unsigned long inPosition, bool inReplace)
{
    unsigned long theSize;
    if (inReplace)
        theSize = PlatMbsNBCnt(MB((const char*)*this), inPosition) +
                  PlatMbsNBCnt(MB(inString.mString), PlatMbsLen(MB(inString.mString))) + 2;
    else {
        unsigned long theLength = PlatMbsLen(MB((const char*)*this));
        theSize = PlatMbsNBCnt(MB((const char*)*this), theLength) +
                  PlatMbsNBCnt(MB(inString.mString), PlatMbsLen(MB(inString.mString))) + 2;
    }
    unsigned long theAvailable = this->mCapacity - PlatMbsNBCnt(MB((const char*)*this), PlatMbsLen(MB((const char*)*this))) + 2;
    // Fixed: the capacity used to be set to the free space (theAvailable), not the needed size, so the copies
    // below overflowed the new buffer
    if (theSize > theAvailable || theSize > this->mCapacity)
        this->mCapacity = theSize;
    char* theNewString = new char[this->mCapacity];
    PlatMbsNCpy(MB(theNewString), MB((const char*)*this), inPosition);
    char* theInsert = (char*)PlatMbsNInc(MB(theNewString), inPosition);
    strcpy(theInsert, inString.mString);
    char* theRest = (char*)PlatMbsNInc(MB((const char*)*this), inPosition);
    if (!inReplace)
        strcat(theInsert, theRest);
    delete[] this->mString;
    this->mString = theNewString;
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
}

// Inserts a C string at a character position (inReplace drops the old tail); when it does not fit, the new
// buffer gets the needed size
void UWideString::InsertString(const char* inString, unsigned long inPosition, bool inReplace)
{
    unsigned long theSize;
    if (inReplace)
        theSize = PlatMbsNBCnt(MB((const char*)*this), inPosition) + PlatMbsNBCnt(MB(inString), PlatMbsLen(MB(inString))) + 2;
    else {
        unsigned long theLength = PlatMbsLen(MB((const char*)*this));
        theSize = PlatMbsNBCnt(MB((const char*)*this), theLength) + PlatMbsNBCnt(MB(inString), PlatMbsLen(MB(inString))) + 2;
    }
    unsigned long theAvailable = this->mCapacity - PlatMbsNBCnt(MB((const char*)*this), PlatMbsLen(MB((const char*)*this))) + 2;
    // Fixed: the capacity used to be set to the free space (theAvailable), not the needed size, so the copies
    // below overflowed the new buffer
    if (theSize > theAvailable || theSize > this->mCapacity)
        this->mCapacity = theSize;
    char* theNewString = new char[this->mCapacity];
    PlatMbsNCpy(MB(theNewString), MB((const char*)*this), inPosition);
    char* theInsert = (char*)PlatMbsNInc(MB(theNewString), inPosition);
    strcpy(theInsert, inString);
    char* theRest = (char*)PlatMbsNInc(MB((const char*)*this), inPosition);
    if (!inReplace)
        strcat(theInsert, theRest);
    delete[] this->mString;
    this->mString = theNewString;
    this->mLength = PlatMbsLen(MB((const char*)*this));
    this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
}

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
            UConvertLoadedText(this->mString, this->mCapacity);   // port (D6): an old Windows-1252 string
            this->mLength = PlatMbsLen(MB((const char*)*this));
            this->mByteLength = PlatMbsNBCnt(MB((const char*)*this), this->mLength);
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

// Writes the byte size and the bytes, both counting the terminator, to the file; returns the first error
long UWideString::Save(FSFile& inFile) const
{
    unsigned long theSize = PlatMbsNBCnt(MB((const char*)*this), this->GetLength()) + 1;
    unsigned long theWritten;
    long theResult = inFile.Write(theSize);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Write(this->mString, theSize, theWritten);
    if (theResult == kErrorOK && theWritten < theSize)
        theResult = kErrorFailed;
    return theResult;
}

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
            PlatMbsNCpy(MB(theCharacter), thePosition, 1);
            char* theEnd = (char*)PlatMbsNInc(MB(theCharacter), 1);
            strcpy(theEnd, "");
            if (inSeparators.Find(theCharacter, 0) == theNotFound) {
                theBetweenTokens = false;
                theStart = i;
            }
        } else {
            PlatMbsNCpy(MB(theCharacter), thePosition, 1);
            char* theEnd = (char*)PlatMbsNInc(MB(theCharacter), 1);
            strcpy(theEnd, "");
            if (inSeparators.Find(theCharacter, 0) != theNotFound || i == this->mLength) {
                outTokens.push_back(this->GetIndexedSubstring(theStart, i - 1));
                theBetweenTokens = true;
            }
        }
        thePosition = PlatMbsInc(thePosition);
    }
    return true;
}
