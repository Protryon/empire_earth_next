// The original UString (include/lle/UString.h and src/Low-Level Engine/Utilities/UString.cpp at a1775ddf, before
// its searches and tokenizers moved to rust/crates/ee_base/src/ustring), unchanged but for the includes, for the golden
// driver us_golden.cpp (which supplies FSFile, the platform's string functions and the error codes).
// C:\EEX\Low-Level Engine\Utilities\UString.h (RECONSTRUCTION; UString.cpp 0x82c43..0x837ad). An 8-bit string with
// a capacity (mCapacity counts the terminator). Requires <vector> (Tokenize) and windows.h-free declarations only.
#ifndef LLE_USTRING_H
#define LLE_USTRING_H


class FSFile;

// port (decision D6): game text is UTF-8. A string a file gives UString::Load or UWideString::Load is converted from
// Windows-1252 when it is not well-formed UTF-8, or always while a file written before the switch loads (a save of
// version < 0.255: the loader that read the version sets the flag around the load). The buffer is replaced when the
// text grows; ioCapacity follows.
LLE_API void USetLegacyTextLoad(bool inLegacy);
LLE_API bool UIsLegacyTextLoad();
LLE_API void UConvertLoadedText(char*& ioString, unsigned long& ioCapacity);

class LLE_API UString {                    // proven sizeof 0x10 (TCP DetectedGame: UStrings at +0x20, +0x30)
public:
    UString();
    UString(const UString& inString);
    UString(const char* inString);
    UString(unsigned long inCapacity);
    virtual ~UString();
    UString& operator=(const UString& inString);
    UString& operator=(const char* inString);
    // exported ??BUString@@QBEPBDXZ = `mov eax,[ecx+4]; ret`; inlined at call sites (TCP 0x188e)
    operator const char*() const { return this->mString; }
    // exported ?GetLength@UString@@QBEKXZ = `mov eax,[ecx+8]; ret`; inlined in TCP 0x287c
    unsigned long GetLength() const { return this->mLength; }
    // exported, ICF-folded (0x4495, with UWideString::GetLength)
    unsigned long GetAllocatedLength() const { return this->mCapacity; }
    // proven inline: exported COMDATs emitted by the first unit (Error System.cpp, 0x1140..0x1279), members pushed
    // straight from memory
    bool Equals(const UString& inString) const { return strcmp(this->mString, inString.mString) == 0; }
    bool Equals(const UString& inString, unsigned long inCount) const
        { return strncmp(this->mString, inString.mString, inCount) == 0; }
    bool Equals(const char* inString) const { return strcmp(this->mString, inString) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const { return strncmp(this->mString, inString, inCount) == 0; }
    bool EqualsIgnoreCase(const UString& inString) const { return PlatStrICmp(this->mString, inString.mString) == 0; }
    bool EqualsIgnoreCase(const UString& inString, unsigned long inCount) const
        { return PlatStrNICmp(this->mString, inString.mString, inCount) == 0; }
    bool EqualsIgnoreCase(const char* inString) const { return PlatStrICmp(this->mString, inString) == 0; }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const
        { return PlatStrNICmp(this->mString, inString, inCount) == 0; }
#ifdef LOWLEVELENGINE_EXPORTS
    static void SetDefaultAllocationLength(unsigned long inLength) { UString::sDefaultAllocationLength = inLength; }
#else
    static void SetDefaultAllocationLength(unsigned long inLength);
#endif
    unsigned long SizeOf() { return this->mCapacity + 3 * sizeof(unsigned long); }  // 0x1220: mCapacity + 0xc
    // port: the text is UTF-8 (decision D6), so the case change is the platform's (the original's byte-wise tolower/
    // toupper would have broken a multi-byte character under a code-page locale)
    void ToLower() { PlatMbsLwr(reinterpret_cast<unsigned char*>(this->mString)); }
    void ToUpper() { PlatMbsUpr(reinterpret_cast<unsigned char*>(this->mString)); }
    char operator[](int inIndex) const;
    UString& operator+=(const UString& inString);
    UString& operator+=(const char* inString);
    void Compact();
    UString& Copy(const UString& inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize);
    UString& Copy(const char* inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize);
    // Find/FindBackwards return GetLength() when not found
    unsigned long Find(const UString& inString, unsigned long inStart) const;
    unsigned long Find(const char* inString, unsigned long inStart) const;
    unsigned long FindBackwards(const UString& inString, unsigned long inStart) const;
    unsigned long FindBackwards(const char* inString, unsigned long inStart) const;
    unsigned long GetBytesUsedToSave() const;
    UString GetSubstring(unsigned long inStart, unsigned long inLength) const;
    UString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const;
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    bool Tokenize(const UString& inSeparators, std::vector<UString>& outTokens) const;
    bool NestedTokenize(const UString& inSeparators, const UString& inBrackets,
                        std::vector<UString>& outTokens) const;
private:
    // friends: the comparisons push mString straight from memory (an inline accessor would load it first)
    friend LLE_API bool operator==(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator==(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator!=(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator!=(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator<(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator<(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator<=(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator<=(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator>(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator>(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator>=(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator>=(const UString& inLeft, const char* inRight);
    friend LLE_API long sprintf(UString& outString, const char* inFormat, ...);   // sets mLength

    char* mString;                          // +0x04
    unsigned long mLength;                  // +0x08
    unsigned long mCapacity;                // +0x0c allocated bytes (including the terminator)

    // Inferred engine-only visibility: see TSScheduler.h (a static data member consumes a VC6 `$E` number in
    // every unit that sees the class; the plug-ins' numbering shows they saw none)
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long sDefaultAllocationLength;   // exported ?sDefaultAllocationLength@UString@@0KA (0x10da3c)
#endif
};

LLE_API UString operator+(const UString& inLeft, const UString& inRight);
LLE_API UString operator+(const UString& inLeft, const char* inRight);
LLE_API bool operator==(const UString& inLeft, const UString& inRight);
LLE_API bool operator==(const UString& inLeft, const char* inRight);
LLE_API bool operator!=(const UString& inLeft, const UString& inRight);
LLE_API bool operator!=(const UString& inLeft, const char* inRight);
LLE_API bool operator<(const UString& inLeft, const UString& inRight);
LLE_API bool operator<(const UString& inLeft, const char* inRight);
LLE_API bool operator<=(const UString& inLeft, const UString& inRight);
LLE_API bool operator<=(const UString& inLeft, const char* inRight);
LLE_API bool operator>(const UString& inLeft, const UString& inRight);
LLE_API bool operator>(const UString& inLeft, const char* inRight);
LLE_API bool operator>=(const UString& inLeft, const UString& inRight);
LLE_API bool operator>=(const UString& inLeft, const char* inRight);
LLE_API long sprintf(UString& outString, const char* inFormat, ...) PLAT_PRINTF_FORMAT(2, 3);

#endif
// C:\EEX\Low-Level Engine\Utilities\UString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


unsigned long UString::sDefaultAllocationLength = 256;

// port (decision D6): the conversion of old Windows-1252 inputs (UString.h)
static bool gULegacyTextLoad = false;

void USetLegacyTextLoad(bool inLegacy)
{
    gULegacyTextLoad = inLegacy;
}

bool UIsLegacyTextLoad()
{
    return gULegacyTextLoad;
}

void UConvertLoadedText(char*& ioString, unsigned long& ioCapacity)
{
    unsigned long theLength = strlen(ioString);
    if (!gULegacyTextLoad && PlatUtf8IsValid(ioString, theLength))
        return;
    uint32_t theNeeded = PlatCp1252ToUtf8(ioString, theLength, NULL, 0);
    char* theText = new char[theNeeded + 1];
    PlatCp1252ToUtf8(ioString, theLength, theText, theNeeded + 1);
    delete[] ioString;
    ioString = theText;
    ioCapacity = theNeeded + 1;
}

// Returns a new string holding the left string followed by the right one
UString operator+(const UString& inLeft, const UString& inRight)
{
    UString theResult(inLeft.GetLength() + inRight.GetLength() + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// Returns a new string holding the string followed by the C string
UString operator+(const UString& inLeft, const char* inRight)
{
    UString theResult(inLeft.GetLength() + strlen(inRight) + 1);
    theResult = inLeft;
    theResult += inRight;
    return theResult;
}

// Returns whether the two strings are equal (case-sensitive)
bool operator==(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) == 0;
}

// Returns whether the string equals the C string (case-sensitive)
bool operator==(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) == 0;
}

// Returns whether the two strings differ (case-sensitive)
bool operator!=(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) != 0;
}

// Returns whether the string differs from the C string (case-sensitive)
bool operator!=(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) != 0;
}

// Returns whether the left string sorts before the right one (strcmp order)
bool operator<(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) < 0;
}

// Returns whether the string sorts before the C string (strcmp order)
bool operator<(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) < 0;
}

// Returns whether the left string sorts before or equal to the right one (strcmp order)
bool operator<=(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) <= 0;
}

// Returns whether the string sorts before or equal to the C string (strcmp order)
bool operator<=(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) <= 0;
}

// Returns whether the left string sorts after the right one (strcmp order)
bool operator>(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) > 0;
}

// Returns whether the string sorts after the C string (strcmp order)
bool operator>(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) > 0;
}

// Returns whether the left string sorts after or equal to the right one (strcmp order)
bool operator>=(const UString& inLeft, const UString& inRight)
{
    return strcmp(inLeft.mString, inRight.mString) >= 0;
}

// Returns whether the string sorts after or equal to the C string (strcmp order)
bool operator>=(const UString& inLeft, const char* inRight)
{
    return strcmp(inLeft.mString, inRight) >= 0;
}

// Formats into the string's buffer with no capacity check, then updates its length; returns vsprintf's result
long sprintf(UString& outString, const char* inFormat, ...)
{
    va_list theArguments;
    va_start(theArguments, inFormat);
    long theResult = vsprintf(outString.mString, inFormat, theArguments);
    outString.mLength = strlen(outString.mString);
    return theResult;
}

// Creates an empty string with the default capacity (256 bytes)
UString::UString()
{
    this->mCapacity = UString::sDefaultAllocationLength;
    this->mString = new char[UString::sDefaultAllocationLength];
    this->mLength = 0;
    this->mString[0] = 0;
}

// Creates a copy of a string with the same capacity
UString::UString(const UString& inString)
{
    this->mLength = inString.mLength;
    this->mCapacity = inString.mCapacity;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString.mString);
}

// Creates a string from a C string, sized exactly to fit it
UString::UString(const char* inString)
{
    this->mLength = strlen(inString);
    this->mCapacity = this->mLength + 1;
    this->mString = new char[this->mCapacity];
    strcpy(this->mString, inString);
}

// Creates an empty string with the given capacity in bytes
UString::UString(unsigned long inCapacity)
{
    this->mCapacity = inCapacity;
    this->mString = new char[inCapacity];
    this->mLength = 0;
    this->mString[0] = 0;
}

// Destroys the string, freeing its buffer
UString::~UString()
{
    delete[] this->mString;
}

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

// Returns the character at an index, or 0 when the index is out of range
char UString::operator[](int inIndex) const
{
    if (inIndex >= 0 && inIndex < (int)this->mLength)
        return this->mString[inIndex];
    return 0;
}

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

// Returns the bytes Save writes: a 4-byte length plus the characters and terminator
unsigned long UString::GetBytesUsedToSave() const
{
    return this->mLength + 5;
}

// Returns inLength characters starting at inStart as a new string
UString UString::GetSubstring(unsigned long inStart, unsigned long inLength) const
{
    UString theResult(inLength + 1);
    theResult.Copy(*this, inStart, inLength, 0, false);
    return theResult;
}

// Returns the characters from inFirst through inLast (inclusive) as a new string
UString UString::GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
{
    unsigned long theLength = inLast - inFirst;
    UString theResult(theLength + 2);
    theResult.Copy(*this, inFirst, theLength + 1, 0, false);
    return theResult;
}

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
            UConvertLoadedText(this->mString, this->mCapacity);   // port (D6): an old Windows-1252 string
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
